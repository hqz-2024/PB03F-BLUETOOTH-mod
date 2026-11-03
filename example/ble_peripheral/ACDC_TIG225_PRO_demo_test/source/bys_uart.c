#include "bys_uart.h"
#include "uart.h"
#include "OSAL.h"
#include "log.h"
#include "pwrmgr.h"

#define BYS_TX_BUF_SIZE     32u
#define TX_QUEUE_SIZE       3u
#define BYS_RX_BUF_SIZE     64u
#define BYS_RX_BUF_MASK     (BYS_RX_BUF_SIZE - 1u)

static uint8  s_tx_hw_buf[BYS_TX_BUF_SIZE];
static uint8  s_task_id;
static uint16 s_rx_evt;
static uint16 s_tx_next_evt;
static bys_uart_rx_cb_t s_rx_cb;
static uint8  s_tx_busy;
static uint8  s_rx_buf[BYS_RX_BUF_SIZE];
static volatile uint16 s_rx_head;
static volatile uint16 s_rx_tail;
static uint8  s_query_idx;
static uint8  s_tx_queue[TX_QUEUE_SIZE][BYS_PKT_LEN];
static uint8  s_tx_head;
static uint8  s_tx_tail;
static uint8  s_poll_pkt[BYS_PKT_LEN];

bys_device_state_t g_bys_state = {0};

/* ACDC TIG225 query order exactly matches the protocol table. */
static const uint16 s_query_cmds[BYS_QUERY_COUNT] = {
    0x0002u, 0x0003u, 0x0004u, 0x0005u, 0x0006u, 0x0007u,
    0x0008u, 0x0009u, 0x000Au, 0x000Bu, 0x000Cu, 0x000Du,
    0x000Eu, 0x000Fu, 0x0010u, 0x0011u, 0x0012u, 0x0013u,
    0x0014u, 0x0015u, 0x0016u
};

uint8 bys_packet_valid(const uint8 *pkt)
{
    uint16 cmd;
    uint16 data;
    uint16 checksum;

    if (pkt == NULL) {
        return 0;
    }

    cmd = BUILD_UINT16(pkt[4], pkt[5]);
    data = BUILD_UINT16(pkt[6], pkt[7]);
    checksum = BUILD_UINT16(pkt[8], pkt[9]);

    return (pkt[0] == BYS_HEADER_0 &&
            pkt[1] == BYS_HEADER_1 &&
            pkt[10] == BYS_TAIL_0 &&
            pkt[11] == BYS_TAIL_1 &&
            checksum == (uint16)(cmd + data)) ? 1u : 0u;
}

uint16 bys_uart_query_cmd(uint8 index)
{
    if (index >= BYS_QUERY_COUNT) {
        return 0u;
    }
    return s_query_cmds[index];
}

static uint16 rx_count(void)
{
    return (uint16)((s_rx_head - s_rx_tail) & BYS_RX_BUF_MASK);
}

static void rx_drop(uint8 count)
{
    s_rx_tail = (uint16)((s_rx_tail + count) & BYS_RX_BUF_MASK);
}

static void rx_peek(uint16 offset, uint8 *dst, uint8 len)
{
    uint8 i;

    for (i = 0u; i < len; i++) {
        dst[i] = s_rx_buf[(s_rx_tail + offset + i) & BYS_RX_BUF_MASK];
    }
}

static void rx_push_from_isr(const uint8 *src, uint8 len)
{
    uint8 i;

    for (i = 0u; i < len; i++) {
        uint16 next_head = (uint16)((s_rx_head + 1u) & BYS_RX_BUF_MASK);

        if (next_head == s_rx_tail) {
            break;
        }
        s_rx_buf[s_rx_head] = src[i];
        s_rx_head = next_head;
    }
    osal_set_event(s_task_id, s_rx_evt);
}

static void build_packet(uint8 *pkt, uint16 dev_type, uint16 cmd, uint16 data)
{
    uint16 checksum = (uint16)(cmd + data);

    pkt[0] = BYS_HEADER_0;
    pkt[1] = BYS_HEADER_1;
    pkt[2] = LO_UINT16(dev_type);
    pkt[3] = HI_UINT16(dev_type);
    pkt[4] = LO_UINT16(cmd);
    pkt[5] = HI_UINT16(cmd);
    pkt[6] = LO_UINT16(data);
    pkt[7] = HI_UINT16(data);
    pkt[8] = LO_UINT16(checksum);
    pkt[9] = HI_UINT16(checksum);
    pkt[10] = BYS_TAIL_0;
    pkt[11] = BYS_TAIL_1;
}

void bys_uart_emit_response(uint16 cmd, uint16 data)
{
    uint8 pkt[BYS_PKT_LEN];

    build_packet(pkt, BYS_DEV_TYPE_TIG225, cmd, data);
    if (s_rx_cb != NULL) {
        s_rx_cb(pkt);
    }
}

static uint8 tx_enqueue(const uint8 *pkt)
{
    uint8 next_tail = (uint8)((s_tx_tail + 1u) % TX_QUEUE_SIZE);

    if (next_tail == s_tx_head) {
        return 1u;
    }
    osal_memcpy(s_tx_queue[s_tx_tail], pkt, BYS_PKT_LEN);
    s_tx_tail = next_tail;
    return 0u;
}

static uint8 *tx_dequeue(void)
{
    uint8 *pkt;

    if (s_tx_head == s_tx_tail) {
        return NULL;
    }
    pkt = s_tx_queue[s_tx_head];
    s_tx_head = (uint8)((s_tx_head + 1u) % TX_QUEUE_SIZE);
    return pkt;
}

static uint8 tx_process(void)
{
    uint8 *pkt;

    if (s_tx_busy) {
        return 0u;
    }
    pkt = tx_dequeue();
    if (pkt == NULL) {
        return 0u;
    }
    s_tx_busy = 1u;
    hal_uart_send_buff(BYS_UART_PORT, pkt, BYS_PKT_LEN);
    return 1u;
}

static void update_state_from_response(uint16 cmd, uint16 data)
{
    switch (cmd) {
    case BYS_RSP_MODE:
        g_bys_state.mode = data;
        break;
    case BYS_RSP_HOT_START:
        g_bys_state.hot_start = data;
        break;
    case BYS_RSP_THRUST:
        g_bys_state.thrust = data;
        break;
    case BYS_RSP_VRD:
        g_bys_state.vrd = data;
        break;
    case BYS_RSP_WELD_CURRENT:
        g_bys_state.weld_current = data;
        break;
    case BYS_RSP_T2T4_SPOT:
        g_bys_state.t2t4_spot = data;
        break;
    case BYS_RSP_PULSE:
        g_bys_state.pulse = data;
        break;
    case BYS_RSP_SPOT_TIME:
        g_bys_state.spot_time = data;
        break;
    case BYS_RSP_PRE_GAS:
        g_bys_state.pre_gas = data;
        break;
    case BYS_RSP_START_CURRENT:
        g_bys_state.start_current = data;
        break;
    case BYS_RSP_UP_SLOPE:
        g_bys_state.up_slope = data;
        break;
    case BYS_RSP_PULSE_DUTY:
        g_bys_state.pulse_duty = data;
        break;
    case BYS_RSP_PULSE_FREQ:
        g_bys_state.pulse_freq = data;
        break;
    case BYS_RSP_PULSE_VALLEY:
        g_bys_state.pulse_valley = data;
        break;
    case BYS_RSP_AC_FREQ:
        g_bys_state.ac_freq = data;
        break;
    case BYS_RSP_AC_DUTY:
        g_bys_state.ac_duty = data;
        break;
    case BYS_RSP_DOWN_SLOPE:
        g_bys_state.down_slope = data;
        break;
    case BYS_RSP_END_CURRENT:
        g_bys_state.end_current = data;
        break;
    case BYS_RSP_POST_GAS:
        g_bys_state.post_gas = data;
        break;
    case BYS_RSP_ALARM:
        g_bys_state.alarm = data;
        break;
    case BYS_RSP_INPUT_VOLTAGE:
        g_bys_state.input_voltage = data;
        g_bys_state.valid = 1u;
        break;
    case BYS_RSP_ERROR:
        LOG("[TIG225] device error=0x%04x\n", data);
        break;
    default:
        break;
    }
}

static void uart_event_cb(uart_Evt_t *evt)
{
    if (evt->type == UART_EVT_TYPE_TX_COMPLETED) {
        osal_start_timerEx(s_task_id, s_tx_next_evt, 15u);
        return;
    }

    if (evt->type != UART_EVT_TYPE_RX_DATA &&
        evt->type != UART_EVT_TYPE_RX_DATA_TO) {
        return;
    }

    rx_push_from_isr(evt->data, evt->len);
}

void bys_uart_init(uint8 task_id, uint16 rx_evt, uint16 tx_next_evt,
                   bys_uart_rx_cb_t rx_cb)
{
    uart_Cfg_t cfg = {
        .tx_pin = BYS_UART_TX_PIN,
        .rx_pin = BYS_UART_RX_PIN,
        .rts_pin = GPIO_DUMMY,
        .cts_pin = GPIO_DUMMY,
        .baudrate = BYS_UART_BAUD,
        .use_fifo = TRUE,
        .hw_fwctrl = FALSE,
        .use_tx_buf = TRUE,
        .parity = FALSE,
        .evt_handler = uart_event_cb,
    };

    s_task_id = task_id;
    s_rx_evt = rx_evt;
    s_tx_next_evt = tx_next_evt;
    s_rx_cb = rx_cb;
    s_rx_head = 0u;
    s_rx_tail = 0u;

    hal_uart_init(cfg, BYS_UART_PORT);
    hal_uart_set_tx_buf(BYS_UART_PORT, s_tx_hw_buf, BYS_TX_BUF_SIZE);
    hal_pwrmgr_lock(MOD_UART1);
}

void bys_uart_set_rx_callback(bys_uart_rx_cb_t rx_cb)
{
    s_rx_cb = rx_cb;
}

uint8 bys_uart_tx_process(void)
{
    s_tx_busy = 0u;
    return tx_process();
}

uint8 bys_uart_poll_next(uint8 app_connected)
{
    uint16 dev_type = app_connected ? BYS_DEV_APP_ON : BYS_DEV_APP_OFF;

    if (s_tx_busy) {
        return 1u;
    }
    if (s_tx_head != s_tx_tail) {
        (void)tx_process();
        return 1u;
    }

    build_packet(s_poll_pkt, dev_type, s_query_cmds[s_query_idx], 0u);
    s_query_idx = (uint8)((s_query_idx + 1u) % BYS_QUERY_COUNT);
    s_tx_busy = 1u;
    hal_uart_send_buff(BYS_UART_PORT, s_poll_pkt, BYS_PKT_LEN);
    return 0u;
}

void bys_uart_process_rx(void)
{
    while (rx_count() >= BYS_PKT_LEN) {
        uint8 pkt[BYS_PKT_LEN];

        rx_peek(0u, pkt, BYS_PKT_LEN);
        if (bys_packet_valid(pkt)) {
            uint16 dev_type = BUILD_UINT16(pkt[2], pkt[3]);
            uint16 cmd = BUILD_UINT16(pkt[4], pkt[5]);
            uint16 data = BUILD_UINT16(pkt[6], pkt[7]);

            g_bys_state.device_type = dev_type;
            update_state_from_response(cmd, data);
            rx_drop(BYS_PKT_LEN);
            if (s_rx_cb != NULL) {
                s_rx_cb(pkt);
            }
        } else {
            rx_drop(1u);
        }
    }

    (void)tx_process();
}

uint8 bys_uart_send_app_cmd(uint8 *buf, uint8 len)
{
    if (len != BYS_PKT_LEN || !bys_packet_valid(buf)) {
        return 1u;
    }

    buf[2] = LO_UINT16(BYS_DEV_APP_ON);
    buf[3] = HI_UINT16(BYS_DEV_APP_ON);
    if (tx_enqueue(buf) != 0u) {
        LOG("[TIG225] TX queue full\n");
        return 1u;
    }
    (void)tx_process();
    return 0u;
}
