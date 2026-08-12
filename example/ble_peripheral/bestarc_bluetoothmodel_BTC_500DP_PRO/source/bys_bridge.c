#include "bcomdef.h"
#include "OSAL.h"
#include "linkDB.h"
#include "gatt.h"
#include "hci.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "peripheralMultiConn.h"
#include "gapbondmgr.h"
#include "sbpProfile_ota.h"
#include "devinfoservice.h"
#include "log.h"

#include "bys_bridge.h"
#include "bys_uart.h"
#include "proxy_uart.h"
#include "clock.h"

/* ─── BLE 连接参数 ──────────────────────────────── */
/* 单位 1.25ms：0x0006=7.5ms，0x0190=500ms */
#define DEFAULT_MIN_CONN_INTERVAL   0x0006u
#define DEFAULT_MAX_CONN_INTERVAL   0x0190u
#define DEFAULT_SLAVE_LATENCY       0u
/* 单位 10ms：0x01F4=500 → 5s 超时后进入 WAITING_AFTER_TIMEOUT */
#define DEFAULT_CONN_TIMEOUT        0x01F4u

/* ─── 广播数据（29字节，格式见DEV_PLAN.md） ────── */
static uint8 advertData[31] = {
    /* AD1: Flags */
    0x02, 0x01, 0x06,
    /* AD2: Complete Local Name "BYS" */
    0x04, 0x09, 'B', 'Y', 'S',
    /* AD3: Manufacturer Specific (length=0x16, type=0xFF) */
    0x16, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* MAC placeholder [10-15] */
    0x00, 0x00,  /* device type placeholder [16-17], filled from device response */
    0x00, 0x00,  /* mode    [18-19] */
    0x00, 0x00,  /* 2T/4T  [20-21] */
    0x00, 0x00,  /* current [22-23] */
    0x00, 0x00,  /* postgas [24-25] */
    0x00, 0x00,  /* arc     [26-27] */
    0x00, 0x00,  /* unit    [28-29] */
};

/* Scan Response：不携带额外数据 */
static uint8 scanRspData[1] = { 0x00 };

/* ─── 模块内部状态 ──────────────────────────────── */
static uint8 bys_TaskID;
static uint8 s_ota_notify_count = 0;

/* 轮询运行标志：1=正在轮询，0=已停止轮询 */
static uint8 s_polling = 0;

/* 操作模式标志：1=收到B/C操作指令，暂停轮询，主控优先处理操作指令 */
static uint8 s_op_active = 0;

/* ─── 连接状态检查 ──────────────────────────────── */

/* BLE 是否至少有 1 个连接 */
static uint8 bys_ble_connected(void)
{
    return (GAPRole_Connect_Active_Num() > 0) ? TRUE : FALSE;
}

/* APP 或第三方设备任一在线即视为"有设备连接" */
static uint8 bys_any_connected(void)
{
    return (GAPRole_Connect_Active_Num() > 0 || proxy_uart_is_connected()) ? TRUE : FALSE;
}

/* ─── 轮询控制 ──────────────────────────────────── */

static void bys_start_polling(void)
{
    if (!s_polling) {
        s_polling = 1;
        LOG("[BYS] Polling STARTED\n");
        /* 确保广播开启：第三方设备唤醒时 APP 可能未连接，必须保持广播以等 APP 后续连接 */
        {
            uint8 en = TRUE;
            GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &en);
        }
        osal_start_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT, BYS_POLL_INTERVAL_MS);
    }
}

static void bys_stop_polling(void)
{
    if (s_polling) {
        s_polling = 0;
        osal_stop_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT);
        LOG("[BYS] Polling STOPPED\n");
    }
}

/* ─── 操作模式控制（B/C 指令优先） ──────────────── */

/*
   收到 B 或 C 的操作指令时调用：
   - 暂停轮询定时器，主控不再收到查询，专注处理操作指令
   - 重启 300ms 操作超时定时器（连续指令会不断刷新窗口）
   - 300ms 无新指令 → BYS_OP_TIMEOUT_EVT → 恢复轮询
*/
static void bys_enter_op_mode(void)
{
    s_op_active = 1;
    osal_stop_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT);
    osal_start_timerEx(bys_TaskID, BYS_OP_TIMEOUT_EVT, BYS_OP_TIMEOUT_MS);
}

/* ─── 内部函数原型 ──────────────────────────────── */
static void peripheralStateNotificationCB(uint16 connHandle, gaprole_States_t newState);
static void simpleProfileChangeCB(uint8 paramID);
static void bys_update_adv_data(void);
static void bys_notify_app(uint8 *raw_pkt);
static void bys_uart_rx_callback(uint8 *raw_pkt);
static void proxy_uart_rx_callback(uint8 *raw_pkt);
static void proxy_hb_wake_cb(void);
static uint8 is_ota_trigger(const uint8 *pkt);
static void bys_ota_trigger(void);
static void enter_ota_mode(void);

/* 检测是否为OTA触发包（cmd=0xFE00, data=0x00FE，设备类型字段忽略） */
static uint8 is_ota_trigger(const uint8 *pkt)
{
    uint16 cmd  = BUILD_UINT16(pkt[4], pkt[5]);
    uint16 data = BUILD_UINT16(pkt[6], pkt[7]);
    return (cmd == BYS_CMD_OTA_TRIGGER && data == BYS_DATA_OTA_TRIGGER) ? 1 : 0;
}

/* 触发OTA流程：向下位机发逃50ms间隔的通知包，再进入OTA模式 */
static void bys_ota_trigger(void)
{
    if (s_ota_notify_count > 0) return;
    s_ota_notify_count = 3;
    LOG("[OTA] OTA triggered, notifying slave...\n");
    osal_set_event(bys_TaskID, BYS_OTA_NOTIFY_EVT);
}

/* 写寄存器标记OTA模式并软复位，Bootloader启动时识别并进入OTA */
static void enter_ota_mode(void)
{
    LOG("[OTA] Entering OTA mode, rebooting...\n");
    *(volatile uint32*)0x4000f034 = 0x2;
    hal_system_soft_reset();
}

/* 将内部地址顺序转换为显示顺序并写入广播MAC字段 */
static void bys_set_adv_mac_be(const uint8 *addr_le)
{
    advertData[ADV_MAC_OFFSET + 0] = addr_le[5];
    advertData[ADV_MAC_OFFSET + 1] = addr_le[4];
    advertData[ADV_MAC_OFFSET + 2] = addr_le[3];
    advertData[ADV_MAC_OFFSET + 3] = addr_le[2];
    advertData[ADV_MAC_OFFSET + 4] = addr_le[1];
    advertData[ADV_MAC_OFFSET + 5] = addr_le[0];
}

/* ─── 回调结构体 ──────────────────────────────────── */
static gapRolesCBs_t bys_PeripheralCBs = {
    peripheralStateNotificationCB,
    NULL
};

static simpleProfileCBs_t bys_SimpleProfileCBs = {
    simpleProfileChangeCB
};

/* ─── 第三方设备回调 ───────────────────────────── */

/* 第三方设备发来非心跳数据包：透传给主控（UART1） */
static void proxy_uart_rx_callback(uint8 *raw_pkt)
{
    /* OTA 触发包也从第三方设备生效 */
    if (is_ota_trigger(raw_pkt)) {
        bys_ota_trigger();
        return;
    }

    LOG("[PROXY RX] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
        raw_pkt[0],raw_pkt[1],raw_pkt[2],raw_pkt[3],raw_pkt[4],raw_pkt[5],
        raw_pkt[6],raw_pkt[7],raw_pkt[8],raw_pkt[9],raw_pkt[10],raw_pkt[11]);

    /* C 发指令前清空 UART0 TX 队列：
       丢弃积压的轮询响应，保证 C 收到的第一条数据是主控对本次指令的确认回复 */
    proxy_uart_flush_tx();

    /* 进入操作模式：暂停轮询，主控优先处理本指令 */
    bys_enter_op_mode();

    /* 透传给主控 */
    if (bys_uart_send_app_cmd(raw_pkt, BYS_PKT_LEN) != 0) {
        LOG("[PROXY] Failed to forward cmd to master\n");
    }
}

/* 心跳唤醒：第三方设备首次上线，启动轮询 */
static void proxy_hb_wake_cb(void)
{
    LOG("[PROXY] Device online (heartbeat detected)\n");
    bys_start_polling();
}

/* ─── 初始化 ─────────────────────────────────────── */
void BYS_Bridge_Init(uint8 task_id)
{
    bys_TaskID = task_id;

    /* 设置GAP参数 */
    uint8  adv_enable  = TRUE;
    uint8  adv_type    = GAP_ADTYPE_ADV_IND;  /* 可连接无向广播 */
    uint16 adv_off     = 0;                    /* 断连后不自动停止广播 */
    uint16 adv_int     = 160;                  /* 100ms = 160 × 0.625ms */
    uint16 min_intv    = DEFAULT_MIN_CONN_INTERVAL;
    uint16 max_intv    = DEFAULT_MAX_CONN_INTERVAL;
    uint16 latency     = DEFAULT_SLAVE_LATENCY;
    uint16 timeout     = DEFAULT_CONN_TIMEOUT;

    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED,    sizeof(uint8),  &adv_enable);
    GAPRole_SetParameter(GAPROLE_ADV_EVENT_TYPE,    sizeof(uint8),  &adv_type);
    GAPRole_SetParameter(GAPROLE_ADVERT_DATA,       sizeof(advertData), advertData);
    GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA,     sizeof(scanRspData), scanRspData);
    GAPRole_SetParameter(GAPROLE_ADVERT_OFF_TIME,   sizeof(uint16), &adv_off);

    /* 广播间隔通过 GAP_SetParamValue 设置 */
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, adv_int);
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, adv_int);

    /* 连接参数 */
    GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL,  sizeof(uint16), &min_intv);
    GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL,  sizeof(uint16), &max_intv);
    GAPRole_SetParameter(GAPROLE_SLAVE_LATENCY,      sizeof(uint16), &latency);
    GAPRole_SetParameter(GAPROLE_TIMEOUT_MULTIPLIER, sizeof(uint16), &timeout);

    /* 注册 GAP/GATT 基础服务 */
    GGS_AddService(GATT_ALL_SERVICES);
    GATTServApp_AddService(GATT_ALL_SERVICES);

    /* 注册 Device Information Service */
    DevInfo_AddService();
    DevInfo_SetParameter(DEVINFO_MODEL_NUMBER,      9,  "BTC500DP");
    DevInfo_SetParameter(DEVINFO_MANUFACTURER_NAME, 3,  "BYS");
    DevInfo_SetParameter(DEVINFO_FIRMWARE_REV,      5,  "1.0.0");
    DevInfo_SetParameter(DEVINFO_HARDWARE_REV,      5,  "1.0.0");
    DevInfo_SetParameter(DEVINFO_SOFTWARE_REV,      5,  "1.0.0");

    /* 注册GATT Profile，不需要配对绑定 */
    SimpleProfile_AddService(GATT_ALL_SERVICES);
    SimpleProfile_RegisterAppCBs(&bys_SimpleProfileCBs);

    /* 初始化 UART1 模块（主控通讯），注册每包响应回调 */
    bys_uart_init(bys_TaskID, BYS_UART_RX_EVT, BYS_UART_TX_NEXT_EVT, bys_uart_rx_callback);

    /* 初始化 UART0 模块（第三方设备透传），注册数据回调和心跳唤醒回调 */
    proxy_uart_init(bys_TaskID, PROXY_UART_RX_EVT, PROXY_HB_TIMEOUT_EVT,
                    PROXY_UART_TX_NEXT_EVT,
                    proxy_uart_rx_callback, proxy_hb_wake_cb);

    /* 触发启动事件 */
    osal_set_event(bys_TaskID, BYS_START_DEVICE_EVT);
}

/* ─── 事件处理主循环 ─────────────────────────────── */
uint16 BYS_Bridge_ProcessEvent(uint8 task_id, uint16 events)
{
    (void)task_id;

    /* 启动GAP角色 */
    if (events & BYS_START_DEVICE_EVT) {
        GAPRole_StartDevice(&bys_PeripheralCBs);
        return events ^ BYS_START_DEVICE_EVT;
    }

    /* 断连后重启广播 */
    if (events & BYS_RESET_ADV_EVT) {
        uint8 en = TRUE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &en);
        return events ^ BYS_RESET_ADV_EVT;
    }

    /* 轮询定时器：操作模式下不轮询（主控优先处理B/C指令），非操作模式才发查询 */
    if (events & BYS_POLL_TIMER_EVT) {
        if (!s_op_active && s_polling) {
            bys_uart_poll_next(bys_any_connected());
        }
        return events ^ BYS_POLL_TIMER_EVT;
    }

    /* 向下位机发送OTA通知包（共发3包，每50ms一包，完成后进入OTA模式） */
    if (events & BYS_OTA_NOTIFY_EVT) {
        if (s_ota_notify_count > 0) {
            uint8 pkt[BYS_PKT_LEN] = {
                BYS_HEADER_0, BYS_HEADER_1, 0x00, 0x80,
                0x00, 0xFE, 0xFE, 0x00, 0xFE, 0xFE,
                BYS_TAIL_0, BYS_TAIL_1
            };
            bys_uart_send_app_cmd(pkt, BYS_PKT_LEN);
            LOG("[OTA] Notify slave %d/3\n", 4 - s_ota_notify_count);
            s_ota_notify_count--;
            osal_start_timerEx(bys_TaskID, BYS_OTA_NOTIFY_EVT, 50);
        } else {
            enter_ota_mode();
        }
        return events ^ BYS_OTA_NOTIFY_EVT;
    }

    /* UART1 收到下位机数据 → 解析并透传给 APP 和第三方设备 */
    if (events & BYS_UART_RX_EVT) {
        bys_uart_process_rx();
        bys_update_adv_data();
        return events ^ BYS_UART_RX_EVT;
    }

    /* UART1 TX 完成 → 优先发队列；队列空且非操作模式则 100ms 后发下一轮查询 */
    if (events & BYS_UART_TX_NEXT_EVT) {
        if (bys_uart_tx_process() == 0) {
            if (!s_op_active && s_polling) {
                osal_start_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT, BYS_POLL_INTERVAL_MS);
            }
        }
        return events ^ BYS_UART_TX_NEXT_EVT;
    }

    /* 操作模式超时：300ms 无新操作指令，恢复轮询 */
    if (events & BYS_OP_TIMEOUT_EVT) {
        s_op_active = 0;
        if (s_polling && bys_any_connected()) {
            osal_start_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT, BYS_POLL_INTERVAL_MS);
        }
        return events ^ BYS_OP_TIMEOUT_EVT;
    }

    /* UART0 收到第三方设备数据 → 解析处理 */
    if (events & PROXY_UART_RX_EVT) {
        proxy_uart_process_rx();
        return events ^ PROXY_UART_RX_EVT;
    }

    /* UART0 TX 完成 → 清 busy 并发队列下一包 */
    if (events & PROXY_UART_TX_NEXT_EVT) {
        proxy_uart_tx_process();
        return events ^ PROXY_UART_TX_NEXT_EVT;
    }

    /* 第三方设备心跳超时 → 清除连接状态，检查是否需要停止轮询 */
    if (events & PROXY_HB_TIMEOUT_EVT) {
        if (proxy_uart_handle_timeout()) {
            LOG("[PROXY] Device offline (heartbeat timeout)\n");
            if (!bys_any_connected()) {
                bys_stop_polling();
            }
        }
        return events ^ PROXY_HB_TIMEOUT_EVT;
    }

    return 0;
}

/* ─── GAP 状态回调（多连接：附带 connHandle） ────── */
static void peripheralStateNotificationCB(uint16 connHandle, gaprole_States_t newState)
{
    (void)connHandle;
    switch (newState) {
    case GAPROLE_STARTED: {
        /* 读取本机MAC并填入广播数据 */
        uint8 addr[B_ADDR_LEN];
        GAPRole_GetParameter(GAPROLE_BD_ADDR, addr);
        bys_set_adv_mac_be(addr);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
        /* 广播启动，但轮询在首次设备连接或心跳时才启动 */
        LOG("[BYS] Advertising started (polling idle)\n");
        /*
           GAPROLE_STARTED 可能由断连后自动重启广播触发，
           此时 GAPRole_Connect_Active_Num 已可靠归零。
           如果 WAITING 阶段未能停止轮询（连接数还没减），在此补齐。
        */
        if (!bys_any_connected()) {
            bys_stop_polling();
        }
        break;
    }
    case GAPROLE_CONNECTED:
    case GAPROLE_CONNECTED_ADV:
    case GAPROLE_CONNECTED_TO_TERMINA:
        LOG("[BYS] Connected, active=%d\n", GAPRole_Connect_Active_Num());
        LOG("[BYS] service changed ind ret=%d (conn=%d)\n",
            GATTServApp_SendServiceChangedInd(connHandle, bys_TaskID), connHandle);
        /* APP 上线 → 启动轮询 */
        bys_start_polling();
        break;

    case GAPROLE_WAITING:
    case GAPROLE_WAITING_AFTER_TIMEOUT:
        /* peripheralMultiConn 会自动重启广播，此处仅日志 */
        LOG("[BYS] Disconnected, active=%d\n", GAPRole_Connect_Active_Num());
        /* 如果第三方设备也不在线 → 停止轮询 */
        if (!bys_any_connected()) {
            bys_stop_polling();
        }
        break;

    default:
        break;
    }
}

/* ─── GATT CHAR1(FFE1) / CHAR2(FFE2) 写入回调 ─── */
static void simpleProfileChangeCB(uint8 paramID)
{
    uint8 buf[SIMPLEPROFILE_CHAR1_LEN];

    if (paramID == SIMPLEPROFILE_CHAR1) {
        SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR1, buf);
        LOG("[APP RX] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
            buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11]);
    } else if (paramID == SIMPLEPROFILE_CHAR2) {
        SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR2, buf);
        LOG("[REMOTE RX] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
            buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11]);
    } else {
        return;
    }

    if (is_ota_trigger(buf)) {
        bys_ota_trigger();
        return;
    }

    /* 进入操作模式：暂停轮询，主控优先处理本指令 */
    bys_enter_op_mode();

    if (bys_uart_send_app_cmd(buf, BYS_PKT_LEN) != 0) {
        LOG("[BYS] Failed to send cmd\n");
    }
}

/* ─── 更新广播数据中的设备状态字段 ─────────────── */
static void bys_update_adv_data(void)
{
    /* 小端序填写各字段 */
#define PUT_LE16(off, val) \
    advertData[(off)]   = LO_UINT16(val); \
    advertData[(off)+1] = HI_UINT16(val)

    PUT_LE16(ADV_DEV_TYPE_OFFSET, g_bys_state.device_type);
    PUT_LE16(ADV_MODE_OFFSET,    g_bys_state.mode);
    PUT_LE16(ADV_T2T4_OFFSET,    g_bys_state.t2t4);
    PUT_LE16(ADV_CURRENT_OFFSET, g_bys_state.current);
    PUT_LE16(ADV_POSTGAS_OFFSET, g_bys_state.postgas);
    PUT_LE16(ADV_ARC_OFFSET,     g_bys_state.arc);
    PUT_LE16(ADV_UNIT_OFFSET,    g_bys_state.unit);
#undef PUT_LE16

    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
}

/* ─── Notify 所有上位机：App CHAR1，遥控器 CHAR2 ─── */
static void bys_notify_app(uint8 *raw_pkt)
{
    if (bys_ble_connected()) {
        LOG("[TX ALL] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
            raw_pkt[0],raw_pkt[1],raw_pkt[2],raw_pkt[3],raw_pkt[4],raw_pkt[5],
            raw_pkt[6],raw_pkt[7],raw_pkt[8],raw_pkt[9],raw_pkt[10],raw_pkt[11]);
        simpleProfile_Notify(SIMPLEPROFILE_CHAR1, BYS_PKT_LEN, raw_pkt);
        simpleProfile_Notify(SIMPLEPROFILE_CHAR2, BYS_PKT_LEN, raw_pkt);
    }
}

/* ─── UART1 RX 回调：下位机每返回一包同时透传给 APP 和第三方设备 ─── */
static void bys_uart_rx_callback(uint8 *raw_pkt)
{
    if (is_ota_trigger(raw_pkt)) {
        bys_ota_trigger();
        return;
    }
    /* 透传给 APP（BLE） */
    bys_notify_app(raw_pkt);
    /* 同时透传给第三方设备（UART0） */
    proxy_uart_send(raw_pkt, BYS_PKT_LEN);
}
