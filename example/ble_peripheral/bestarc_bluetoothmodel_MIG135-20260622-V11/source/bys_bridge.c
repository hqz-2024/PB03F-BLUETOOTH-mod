#include "bcomdef.h"
#include "OSAL.h"
#include "linkDB.h"
#include "gatt.h"
#include "hci.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "peripheral.h"
#include "gapbondmgr.h"
#include "sbpProfile_ota.h"
#include "devinfoservice.h"
#include "log.h"

#include "bys_bridge.h"
#include "bys_uart.h"
#include "clock.h"

/* ─── BLE 连接参数 ──────────────────────────────── */
/* 单位 1.25ms：0x0006=7.5ms，0x0190=500ms */
#define DEFAULT_MIN_CONN_INTERVAL   0x0006u
#define DEFAULT_MAX_CONN_INTERVAL   0x0190u
#define DEFAULT_SLAVE_LATENCY       0u
/* 单位 10ms：0x01F4=500 → 5s 超时后进入 WAITING_AFTER_TIMEOUT */
#define DEFAULT_CONN_TIMEOUT        0x01F4u

/* ─── 广播数据（29字节，MIG145 Pro 状态字段） ────── */
static uint8 advertData[31] = {
    /* AD1: Flags */
    0x02, 0x01, 0x06,
    /* AD2: Complete Local Name "BYS" */
    0x04, 0x09, 'B', 'Y', 'S',
    /* AD3: Manufacturer Specific (length=0x16, type=0xFF) */
    0x16, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* MAC placeholder [10-15] */
    0x00, 0x00,  /* device type    [16-17] */
    0x00, 0x00,  /* mode           [18-19] */
    0x00, 0x00,  /* wire diameter  [20-21] */
    0x00, 0x00,  /* MIG current    [22-23] */
    0x00, 0x00,  /* 2T/4T          [24-25] */
    0x00, 0x00,  /* alarm          [26-27] */
    0x00, 0x00,  /* input voltage  [28-29] */
};

/* Scan Response：不携带额外数据 */
static uint8 scanRspData[1] = { 0x00 };

/* ─── 模块内部状态 ──────────────────────────────── */
static uint8 bys_TaskID;
static uint8 g_connected = FALSE;
static uint8 s_ota_notify_count = 0;

/* ─── 内部函数原型 ──────────────────────────────── */
static void peripheralStateNotificationCB(gaprole_States_t newState);
static void simpleProfileChangeCB(uint8 paramID);
static void bys_update_adv_data(void);
static void bys_notify_app(uint8 *raw_pkt);
static void bys_uart_rx_callback(uint8 *raw_pkt);
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

/* 触发OTA流程：向下位机发送50ms间隔的通知包，再进入OTA模式 */
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

    /* 注册 Device Information Service */
    DevInfo_AddService();
    DevInfo_SetParameter(DEVINFO_MODEL_NUMBER,      6,  "MIG135");
    DevInfo_SetParameter(DEVINFO_MANUFACTURER_NAME, 3,  "BYS");
    DevInfo_SetParameter(DEVINFO_FIRMWARE_REV,      5,  "1.0.0");
    DevInfo_SetParameter(DEVINFO_HARDWARE_REV,      5,  "1.0.0");
    DevInfo_SetParameter(DEVINFO_SOFTWARE_REV,      5,  "1.0.0");

    /* 注册GATT Profile，不需要配对绑定 */
    SimpleProfile_AddService(GATT_ALL_SERVICES);
    SimpleProfile_RegisterAppCBs(&bys_SimpleProfileCBs);

    /* 初始化UART模块，注册每包响应回调 */
    bys_uart_init(bys_TaskID, BYS_UART_RX_EVT, BYS_UART_TX_NEXT_EVT, bys_uart_rx_callback);

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

    /* 轮询定时器：TX队列空闲时触发，向下位机发送下一条查询；若仍busy则等待TX_NEXT_EVT重新调度 */
    if (events & BYS_POLL_TIMER_EVT) {
        bys_uart_poll_next(g_connected);
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

    /* UART 收到下位机数据（解析在 bys_uart_rx_callback 里完成） */
    if (events & BYS_UART_RX_EVT) {
        bys_uart_process_rx();
        bys_update_adv_data();  /* 刷新广播数据 */
        return events ^ BYS_UART_RX_EVT;
    }

    /* 上一包TX完成：优先发队列，队列空闲则100ms后触发轮询 */
    if (events & BYS_UART_TX_NEXT_EVT) {
        if (bys_uart_tx_process() == 0) {
            osal_start_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT, BYS_POLL_INTERVAL_MS);
        }
        return events ^ BYS_UART_TX_NEXT_EVT;
    }

    return 0;
}

/* ─── GAP 状态回调 ───────────────────────────────── */
static void peripheralStateNotificationCB(gaprole_States_t newState)
{
    switch (newState) {
    case GAPROLE_STARTED: {
        /* 读取本机MAC并填入广播数据 */
        uint8 addr[B_ADDR_LEN];
        GAPRole_GetParameter(GAPROLE_BD_ADDR, addr);
        bys_set_adv_mac_be(addr);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
        /* 启动轮询定时器：每100ms查询下位机状态 */
        osal_start_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT, BYS_POLL_INTERVAL_MS);
        LOG("[BYS] Advertising started\n");
        break;
    }
    case GAPROLE_CONNECTED:
        g_connected = TRUE;
        LOG("[BYS] Connected\n");
        break;

    case GAPROLE_WAITING:
    case GAPROLE_WAITING_AFTER_TIMEOUT:
        g_connected = FALSE;
        /* 断连后不停止轮询定时器，继续以 APP_OFF 模式查询下位机 */
        osal_start_timerEx(bys_TaskID, BYS_RESET_ADV_EVT, BYS_RESET_ADV_DELAY_MS);
        LOG("[BYS] Disconnected, re-advertising...\n");
        break;

    default:
        break;
    }
}

/* ─── GATT CHAR1(FFE1) 写入回调（APP→下位机） ─── */
static void simpleProfileChangeCB(uint8 paramID)
{
    if (paramID != SIMPLEPROFILE_CHAR1) return;

    uint8 buf[SIMPLEPROFILE_CHAR1_LEN];  /* 必须与 GetParameter 拷贝长度一致 */
    SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR1, buf);
    // app通讯日志打印代码
    LOG("[APP RX] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
        buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11]);

    if (is_ota_trigger(buf)) {
        bys_ota_trigger();
        return;
    }

    /* 加入发送队列（高优先级），自动修正设备类型字段 */
    if (bys_uart_send_app_cmd(buf, BYS_PKT_LEN) != 0) {
        LOG("[BYS] Failed to send APP cmd\n");
    }
}

/* ─── 更新广播数据中的设备状态字段 ─────────────── */
static void bys_update_adv_data(void)
{
    /* 小端序填写各字段 */
#define PUT_LE16(off, val) \
    advertData[(off)]   = LO_UINT16(val); \
    advertData[(off)+1] = HI_UINT16(val)

    PUT_LE16(ADV_DEV_TYPE_OFFSET,       g_bys_state.device_type);
    PUT_LE16(ADV_MODE_OFFSET,           g_bys_state.mode);
    PUT_LE16(ADV_WIRE_DIAMETER_OFFSET,  g_bys_state.wire_diameter);
    PUT_LE16(ADV_MIG_CURRENT_OFFSET,    g_bys_state.mig_current);
    PUT_LE16(ADV_T2T4_OFFSET,           g_bys_state.t2t4);
    PUT_LE16(ADV_ALARM_OFFSET,          g_bys_state.alarm);
    PUT_LE16(ADV_INPUT_VOLTAGE_OFFSET,  g_bys_state.input_voltage);
#undef PUT_LE16

    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
}

/* ─── Notify APP：通过FFE1发送原始数据包 ─────────── */
static void bys_notify_app(uint8 *raw_pkt)
{
    if (g_connected) {
          // 下位机串口通讯日志打印代码
          LOG("[APP TX] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
            raw_pkt[0],raw_pkt[1],raw_pkt[2],raw_pkt[3],raw_pkt[4],raw_pkt[5],
            raw_pkt[6],raw_pkt[7],raw_pkt[8],raw_pkt[9],raw_pkt[10],raw_pkt[11]);
        simpleProfile_Notify(SIMPLEPROFILE_CHAR1, BYS_PKT_LEN, raw_pkt);
    }
}

/* ─── UART RX 回调：下位机每返回一包立即透传给APP ─── */
static void bys_uart_rx_callback(uint8 *raw_pkt)
{
    // 下位机串口通讯日志打印代码
    LOG("[UART RX] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
        raw_pkt[0],raw_pkt[1],raw_pkt[2],raw_pkt[3],raw_pkt[4],raw_pkt[5],
        raw_pkt[6],raw_pkt[7],raw_pkt[8],raw_pkt[9],raw_pkt[10],raw_pkt[11]);
    if (is_ota_trigger(raw_pkt)) {
        bys_ota_trigger();
        return;
    }
    bys_notify_app(raw_pkt);  /* 立即 Notify，不缓存 */
}
