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
    0x01, 0x00,  /* device type = 0x0001 (BTC500DP PRO) [16-17] */
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
static uint8 g_connected = FALSE;

/* ─── 内部函数原型 ──────────────────────────────── */
static void peripheralStateNotificationCB(gaprole_States_t newState);
static void simpleProfileChangeCB(uint8 paramID);
static void bys_update_adv_data(void);
static void bys_notify_app(uint8 *raw_pkt);
static void bys_uart_rx_callback(uint8 *raw_pkt);

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
    DevInfo_SetParameter(DEVINFO_MODEL_NUMBER,      9,  "BTC500DP");
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

    /* 1s 轮询定时器：向下位机发送下一条查询 */
    if (events & BYS_POLL_TIMER_EVT) {
        /* 若队列满（APP 指令占用），50ms 后重试，不推进查询索引 */
        if (bys_uart_poll_next(g_connected) != 0) {
            osal_start_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT, 50);
        } else {
            osal_start_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT, BYS_POLL_INTERVAL_MS);
        }
        return events ^ BYS_POLL_TIMER_EVT;
    }

    /* UART 收到下位机数据（解析在 bys_uart_rx_callback 里完成） */
    if (events & BYS_UART_RX_EVT) {
        bys_uart_process_rx();
        bys_update_adv_data();  /* 刷新广播数据 */
        return events ^ BYS_UART_RX_EVT;
    }

    /* 上一包TX完成，发送队列中的下一包 */
    if (events & BYS_UART_TX_NEXT_EVT) {
        bys_uart_tx_process();
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
        osal_memcpy(&advertData[ADV_MAC_OFFSET], addr, B_ADDR_LEN);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
        /* 上电即启动轮询，无论是否连接蓝牙都持续查询下位机 */
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

    PUT_LE16(ADV_MODE_OFFSET,    g_bys_state.mode);
    PUT_LE16(ADV_T2T4_OFFSET,    g_bys_state.t2t4);
    PUT_LE16(ADV_CURRENT_OFFSET, g_bys_state.current);
    PUT_LE16(ADV_POSTGAS_OFFSET, g_bys_state.postgas);
    PUT_LE16(ADV_ARC_OFFSET,     g_bys_state.arc);
    PUT_LE16(ADV_UNIT_OFFSET,    g_bys_state.unit);
#undef PUT_LE16

    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
}

/* ─── Notify APP：通过FFE1发送原始数据包 ─────────── */
static void bys_notify_app(uint8 *raw_pkt)
{
    if (g_connected) {
        simpleProfile_Notify(SIMPLEPROFILE_CHAR1, BYS_PKT_LEN, raw_pkt);
    }
}

/* ─── UART RX 回调：下位机每返回一包立即透传给APP ─── */
static void bys_uart_rx_callback(uint8 *raw_pkt)
{
    bys_notify_app(raw_pkt);  /* 立即 Notify，不缓存 */
}
