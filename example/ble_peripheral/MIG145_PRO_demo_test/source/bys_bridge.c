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
#include "gpio.h"
#include "clock.h"

#include "bys_bridge.h"
#include "bys_test.h"
#include "bys_uart.h"

#define DEFAULT_MIN_CONN_INTERVAL   0x0006u
#define DEFAULT_MAX_CONN_INTERVAL   0x0190u
#define DEFAULT_SLAVE_LATENCY       0u
#define DEFAULT_CONN_TIMEOUT        0x01F4u
#define APP_CMD_QUEUE_SIZE          4u

static uint8 advertData[31] = {
    0x02, 0x01, 0x06,
    0x04, 0x09, 'B', 'Y', 'S',
    0x16, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

static uint8 scanRspData[1] = {0x00};
static uint8 bys_TaskID;
static uint8 g_connected;
static uint8 s_ota_notify_count;
static uint8 s_app_cmd_queue[APP_CMD_QUEUE_SIZE][BYS_PKT_LEN];
static uint8 s_app_cmd_head;
static uint8 s_app_cmd_tail;
#ifdef BYS_TEST_MODE
static uint8 g_btn_debounce;
#endif

static void peripheralStateNotificationCB(gaprole_States_t newState);
static void simpleProfileChangeCB(uint8 paramID);
static void bys_update_adv_data(void);
static void bys_notify_app(uint8 *raw_pkt);
static void bys_uart_rx_callback(uint8 *raw_pkt);
static uint8 is_ota_trigger(const uint8 *pkt);
static void bys_ota_trigger(void);
static void enter_ota_mode(void);
static uint8 app_cmd_enqueue(const uint8 *pkt);
static uint8 *app_cmd_dequeue(void);
#ifdef BYS_TEST_MODE
static void bys_button_cb(gpio_pin_e pin, gpio_polarity_e type);
#endif

static uint8 app_cmd_enqueue(const uint8 *pkt)
{
    uint8 next_tail = (uint8)((s_app_cmd_tail + 1u) % APP_CMD_QUEUE_SIZE);

    if (next_tail == s_app_cmd_head) {
        return 1u;
    }
    osal_memcpy(s_app_cmd_queue[s_app_cmd_tail], pkt, BYS_PKT_LEN);
    s_app_cmd_tail = next_tail;
    return 0u;
}

static uint8 *app_cmd_dequeue(void)
{
    uint8 *pkt;

    if (s_app_cmd_head == s_app_cmd_tail) {
        return NULL;
    }
    pkt = s_app_cmd_queue[s_app_cmd_head];
    s_app_cmd_head = (uint8)((s_app_cmd_head + 1u) % APP_CMD_QUEUE_SIZE);
    return pkt;
}

static void bys_set_adv_mac_be(const uint8 *addr_le)
{
    advertData[ADV_MAC_OFFSET + 0u] = addr_le[5];
    advertData[ADV_MAC_OFFSET + 1u] = addr_le[4];
    advertData[ADV_MAC_OFFSET + 2u] = addr_le[3];
    advertData[ADV_MAC_OFFSET + 3u] = addr_le[2];
    advertData[ADV_MAC_OFFSET + 4u] = addr_le[1];
    advertData[ADV_MAC_OFFSET + 5u] = addr_le[0];
}

static gapRolesCBs_t bys_PeripheralCBs = {
    peripheralStateNotificationCB,
    NULL
};

static simpleProfileCBs_t bys_SimpleProfileCBs = {
    simpleProfileChangeCB
};

static uint8 is_ota_trigger(const uint8 *pkt)
{
    uint16 cmd = BUILD_UINT16(pkt[4], pkt[5]);
    uint16 data = BUILD_UINT16(pkt[6], pkt[7]);

    return (cmd == BYS_CMD_OTA_TRIGGER &&
            data == BYS_DATA_OTA_TRIGGER) ? 1u : 0u;
}

static void bys_ota_trigger(void)
{
    if (s_ota_notify_count > 0u) {
        return;
    }
    s_ota_notify_count = 3u;
    LOG("[MIG145] OTA triggered\n");
    osal_set_event(bys_TaskID, BYS_OTA_NOTIFY_EVT);
}

static void enter_ota_mode(void)
{
    LOG("[MIG145] rebooting into OTA mode\n");
    *(volatile uint32 *)0x4000f034 = 0x2;
    hal_system_soft_reset();
}

void BYS_Bridge_Init(uint8 task_id)
{
    uint8 adv_enable = TRUE;
    uint8 adv_type = GAP_ADTYPE_ADV_IND;
    uint16 adv_off = 0u;
    uint16 adv_int = 160u;
    uint16 min_intv = DEFAULT_MIN_CONN_INTERVAL;
    uint16 max_intv = DEFAULT_MAX_CONN_INTERVAL;
    uint16 latency = DEFAULT_SLAVE_LATENCY;
    uint16 timeout = DEFAULT_CONN_TIMEOUT;

    bys_TaskID = task_id;

    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &adv_enable);
    GAPRole_SetParameter(GAPROLE_ADV_EVENT_TYPE, sizeof(uint8), &adv_type);
    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
    GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
    GAPRole_SetParameter(GAPROLE_ADVERT_OFF_TIME, sizeof(uint16), &adv_off);

    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, adv_int);
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, adv_int);

    GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16), &min_intv);
    GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16), &max_intv);
    GAPRole_SetParameter(GAPROLE_SLAVE_LATENCY, sizeof(uint16), &latency);
    GAPRole_SetParameter(GAPROLE_TIMEOUT_MULTIPLIER, sizeof(uint16), &timeout);

    DevInfo_AddService();
    DevInfo_SetParameter(DEVINFO_MODEL_NUMBER,
                         sizeof(DEMO_TEST_MODEL_NAME) - 1u,
                         DEMO_TEST_MODEL_NAME);
    DevInfo_SetParameter(DEVINFO_MANUFACTURER_NAME, 3u, "BYS");
    DevInfo_SetParameter(DEVINFO_FIRMWARE_REV, 5u, "1.0.0");
    DevInfo_SetParameter(DEVINFO_HARDWARE_REV, 5u, "1.0.0");
    DevInfo_SetParameter(DEVINFO_SOFTWARE_REV, 5u, "1.0.0");

    SimpleProfile_AddService(GATT_ALL_SERVICES);
    SimpleProfile_RegisterAppCBs(&bys_SimpleProfileCBs);

    bys_uart_set_rx_callback(bys_uart_rx_callback);
#ifdef BYS_TEST_MODE
    bys_test_init();
    bys_update_adv_data();
#else
    bys_uart_init(bys_TaskID, BYS_UART_RX_EVT, BYS_UART_TX_NEXT_EVT,
                  bys_uart_rx_callback);
#endif

    hal_gpio_pin_init(BYS_LED_PIN, GPIO_OUTPUT);
    hal_gpio_write(BYS_LED_PIN, 0);
    osal_start_timerEx(bys_TaskID, BYS_LED_EVT, BYS_LED_TOGGLE_MS);

#ifdef BYS_TEST_MODE
    hal_gpio_pin_init(BYS_BUTTON_PIN, GPIO_INPUT);
    hal_gpio_pull_set(BYS_BUTTON_PIN, GPIO_PULL_DOWN);
    hal_gpioin_register(BYS_BUTTON_PIN, bys_button_cb, NULL);
    hal_gpioin_enable(BYS_BUTTON_PIN);
#endif

    /* Start polling immediately at boot; BLE connection state is not required. */
    osal_start_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT,
                       BYS_POLL_INTERVAL_MS);

    osal_set_event(bys_TaskID, BYS_START_DEVICE_EVT);
}

uint16 BYS_Bridge_ProcessEvent(uint8 task_id, uint16 events)
{
    (void)task_id;

    if (events & BYS_START_DEVICE_EVT) {
        GAPRole_StartDevice(&bys_PeripheralCBs);
        return events ^ BYS_START_DEVICE_EVT;
    }

    if (events & BYS_RESET_ADV_EVT) {
        uint8 enabled = TRUE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &enabled);
        return events ^ BYS_RESET_ADV_EVT;
    }

    if (events & BYS_APP_CMD_EVT) {
        uint8 *cmd = app_cmd_dequeue();

        if (cmd != NULL) {
#ifdef BYS_TEST_MODE
            (void)bys_test_handle_app_cmd(cmd, BYS_PKT_LEN);
#else
            if (bys_uart_send_app_cmd(cmd, BYS_PKT_LEN) != 0u) {
                LOG("[MIG145] failed to forward App command\n");
            }
#endif
            if (s_app_cmd_head != s_app_cmd_tail) {
                osal_set_event(bys_TaskID, BYS_APP_CMD_EVT);
            }
        }
        return events ^ BYS_APP_CMD_EVT;
    }

    if (events & BYS_POLL_TIMER_EVT) {
#ifdef BYS_TEST_MODE
        bys_test_poll_next();
        bys_update_adv_data();
        osal_start_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT,
                           BYS_POLL_INTERVAL_MS);
#else
        (void)bys_uart_poll_next(g_connected);
#endif
        return events ^ BYS_POLL_TIMER_EVT;
    }

#ifdef BYS_TEST_MODE
    if (events & BYS_BUTTON_EVT) {
        g_btn_debounce = FALSE;
        bys_test_tick();
        bys_update_adv_data();
        return events ^ BYS_BUTTON_EVT;
    }
#endif

    if (events & BYS_UART_RX_EVT) {
#ifndef BYS_TEST_MODE
        bys_uart_process_rx();
        bys_update_adv_data();
#endif
        return events ^ BYS_UART_RX_EVT;
    }

    if (events & BYS_UART_TX_NEXT_EVT) {
#ifndef BYS_TEST_MODE
        if (bys_uart_tx_process() == 0u) {
            osal_start_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT,
                               BYS_POLL_INTERVAL_MS);
        }
#endif
        return events ^ BYS_UART_TX_NEXT_EVT;
    }

    if (events & BYS_OTA_NOTIFY_EVT) {
        if (s_ota_notify_count > 0u) {
            uint8 pkt[BYS_PKT_LEN] = {
                BYS_HEADER_0, BYS_HEADER_1, 0x00, 0x80,
                0x00, 0xFE, 0xFE, 0x00, 0xFE, 0xFE,
                BYS_TAIL_0, BYS_TAIL_1
            };
#ifndef BYS_TEST_MODE
            if (bys_uart_send_app_cmd(pkt, BYS_PKT_LEN) != 0u) {
                osal_start_timerEx(bys_TaskID, BYS_OTA_NOTIFY_EVT, 10u);
                return events ^ BYS_OTA_NOTIFY_EVT;
            }
#else
            (void)pkt;
#endif
            s_ota_notify_count--;
            osal_start_timerEx(bys_TaskID, BYS_OTA_NOTIFY_EVT, 50u);
        } else {
            enter_ota_mode();
        }
        return events ^ BYS_OTA_NOTIFY_EVT;
    }

    if (events & BYS_LED_EVT) {
        hal_gpio_write(BYS_LED_PIN, !hal_gpio_read(BYS_LED_PIN));
        osal_start_timerEx(bys_TaskID, BYS_LED_EVT, BYS_LED_TOGGLE_MS);
        return events ^ BYS_LED_EVT;
    }

    return 0u;
}

static void peripheralStateNotificationCB(gaprole_States_t newState)
{
    switch (newState) {
    case GAPROLE_STARTED: {
        uint8 addr[B_ADDR_LEN];
        GAPRole_GetParameter(GAPROLE_BD_ADDR, addr);
        bys_set_adv_mac_be(addr);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
        LOG("[MIG145] advertising started\n");
        break;
    }
    case GAPROLE_CONNECTED:
        g_connected = TRUE;
        LOG("[MIG145] connected\n");
        break;
    case GAPROLE_WAITING:
    case GAPROLE_WAITING_AFTER_TIMEOUT:
        g_connected = FALSE;
        osal_start_timerEx(bys_TaskID, BYS_RESET_ADV_EVT,
                           BYS_RESET_ADV_DELAY_MS);
        LOG("[MIG145] disconnected\n");
        break;
    default:
        break;
    }
}

static void simpleProfileChangeCB(uint8 paramID)
{
    uint8 buf[SIMPLEPROFILE_CHAR1_LEN];

    if (paramID != SIMPLEPROFILE_CHAR1) {
        return;
    }
    if (SimpleProfile_GetLastWriteLen() != SIMPLEPROFILE_CHAR1_LEN) {
        LOG("[MIG145] ignore non-12-byte App write\n");
        return;
    }
    (void)SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR1, buf);
    LOG("[MIG145 APP] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
        buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);

    if (!bys_packet_valid(buf)) {
        LOG("[MIG145] ignore invalid App frame\n");
        return;
    }

    if (is_ota_trigger(buf)) {
        bys_ota_trigger();
        return;
    }

    if (app_cmd_enqueue(buf) != 0u) {
        LOG("[MIG145] App command queue full\n");
        return;
    }
    osal_set_event(bys_TaskID, BYS_APP_CMD_EVT);
}

static void bys_update_adv_data(void)
{
#define PUT_LE16(off, val) \
    do { \
        advertData[(off)] = LO_UINT16(val); \
        advertData[(off) + 1u] = HI_UINT16(val); \
    } while (0)

    PUT_LE16(ADV_DEV_TYPE_OFFSET, g_bys_state.device_type);
    PUT_LE16(ADV_MODE_OFFSET, g_bys_state.mode);
    PUT_LE16(ADV_WIRE_OFFSET, g_bys_state.wire_diameter);
    PUT_LE16(ADV_CURRENT_OFFSET, g_bys_state.mig_current);
    PUT_LE16(ADV_T2T4_OFFSET, g_bys_state.t2t4);
    PUT_LE16(ADV_ALARM_OFFSET, g_bys_state.alarm);
    PUT_LE16(ADV_INPUT_VOLTAGE_OFFSET, g_bys_state.input_voltage);

#undef PUT_LE16

    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
}

static void bys_notify_app(uint8 *raw_pkt)
{
    if (g_connected) {
        (void)simpleProfile_Notify(SIMPLEPROFILE_CHAR1, BYS_PKT_LEN, raw_pkt);
    }
}

static void bys_uart_rx_callback(uint8 *raw_pkt)
{
    LOG("[MIG145 UART] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
        raw_pkt[0], raw_pkt[1], raw_pkt[2], raw_pkt[3],
        raw_pkt[4], raw_pkt[5], raw_pkt[6], raw_pkt[7],
        raw_pkt[8], raw_pkt[9], raw_pkt[10], raw_pkt[11]);

    if (is_ota_trigger(raw_pkt)) {
        bys_ota_trigger();
        return;
    }
    bys_notify_app(raw_pkt);
}

#ifdef BYS_TEST_MODE
static void bys_button_cb(gpio_pin_e pin, gpio_polarity_e type)
{
    (void)pin;
    (void)type;
    if (g_btn_debounce) {
        return;
    }
    g_btn_debounce = TRUE;
    osal_start_timerEx(bys_TaskID, BYS_BUTTON_EVT, BYS_BUTTON_DEBOUNCE_MS);
}
#endif
