#include "bys_uart.h"
#include "uart.h"
#include "OSAL.h"
#include "log.h"
#include "pwrmgr.h"

/* TX 软件缓冲区（用于中断驱动异步发送，避免 BLE 中断打断字节流） */
#define BYS_TX_BUF_SIZE   32
static uint8  s_tx_hw_buf[BYS_TX_BUF_SIZE];

/* ─── 模块内部变量 ─────────────────────────────── */
static uint8  s_task_id;
static uint16 s_rx_evt;
static uint16 s_tx_next_evt;
static bys_uart_rx_cb_t s_rx_cb;
static uint8  s_tx_busy = 0;

/* 接收环形缓冲区，最多缓存3包 */
static uint8  s_rx_buf[BYS_PKT_LEN * 3];
static uint8  s_rx_len = 0;

/* 轮询状态：当前待发送的查询命令索引 */
static uint8  s_query_idx = 0;

/* 发送队列：仅缓存APP→下位机指令，轮询包不入队 */
#define TX_QUEUE_SIZE   3
static uint8  s_tx_queue[TX_QUEUE_SIZE][BYS_PKT_LEN];
static uint8  s_tx_head = 0;
static uint8  s_tx_tail = 0;

/* 轮询包直发缓冲（不入队列），异步发送期间需保持不变 */
static uint8  s_poll_pkt[BYS_PKT_LEN];

/* 统一TX节流：任何UART包发送后置1，定时器到期清零，保证包间≥100ms */
static uint8  s_tx_throttle = 0;

/* 通知队列：缓存下位机→APP的上报数据，由定时器驱动逐包发送 */
#define NOTIFY_QUEUE_SIZE   4
static uint8  s_notify_queue[NOTIFY_QUEUE_SIZE][BYS_PKT_LEN];
static uint8  s_notify_head = 0;
static uint8  s_notify_tail = 0;
static uint8  s_notify_count = 0;

/* 全局设备状态，供广播数据使用 */
bys_device_state_t g_bys_state = {0};

/* 12条查询命令循环表 — MIG145 Pro 协议 */
static const uint16 s_query_cmds[BYS_QUERY_COUNT] = {
    BYS_CMD_QUERY_MODE,
    BYS_CMD_QUERY_WIRE_DIAMETER,
    BYS_CMD_QUERY_SMART_MODE,
    BYS_CMD_QUERY_MIG_CURRENT,
    BYS_CMD_QUERY_MIG_VOLTAGE_ADJ,
    BYS_CMD_QUERY_T2T4,
    BYS_CMD_QUERY_TIG_CURRENT,
    BYS_CMD_QUERY_MMA_CURRENT,
    BYS_CMD_QUERY_VRD,
    BYS_CMD_QUERY_MIG_VOLTAGE,
    BYS_CMD_QUERY_ALARM,
    BYS_CMD_QUERY_INPUT_VOLTAGE,
};

/* ─── 内部函数 ──────────────────────────────────── */

/* 发送队列入队（队满返回1） */
static uint8 tx_enqueue(uint8 *pkt)
{
    uint8 next_tail = (s_tx_tail + 1) % TX_QUEUE_SIZE;
    if (next_tail == s_tx_head) return 1;
    osal_memcpy(s_tx_queue[s_tx_tail], pkt, BYS_PKT_LEN);
    s_tx_tail = next_tail;
    return 0;
}

/* 发送队列出队（队空返回NULL） */
static uint8* tx_dequeue(void)
{
    if (s_tx_head == s_tx_tail) return NULL;
    uint8 *pkt = s_tx_queue[s_tx_head];
    s_tx_head = (s_tx_head + 1) % TX_QUEUE_SIZE;
    return pkt;
}

/* 通知队列入队（队满覆盖最旧） */
static void notify_enqueue(uint8 *pkt)
{
    if (s_notify_count >= NOTIFY_QUEUE_SIZE) {
        s_notify_head = (s_notify_head + 1) % NOTIFY_QUEUE_SIZE;
        s_notify_count--;
    }
    osal_memcpy(s_notify_queue[s_notify_tail], pkt, BYS_PKT_LEN);
    s_notify_tail = (s_notify_tail + 1) % NOTIFY_QUEUE_SIZE;
    s_notify_count++;
}

/* 通知队列出队（队空返回NULL） */
static uint8* notify_dequeue(void)
{
    if (s_notify_count == 0) return NULL;
    uint8 *pkt = s_notify_queue[s_notify_head];
    s_notify_head = (s_notify_head + 1) % NOTIFY_QUEUE_SIZE;
    s_notify_count--;
    return pkt;
}

/* 尝试发送队列头部一包。统一节流：busy或throttle时跳过 */
static void tx_process(void)
{
    if (s_tx_busy) return;
    if (s_tx_throttle) return;
    uint8 *pkt = tx_dequeue();
    if (pkt == NULL) return;
    s_tx_busy = 1;
    s_tx_throttle = 1;
    // 下位机串口通讯日志打印代码
    LOG("[UART TX] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
        pkt[0],pkt[1],pkt[2],pkt[3],pkt[4],pkt[5],pkt[6],pkt[7],pkt[8],pkt[9],pkt[10],pkt[11]);
   
    hal_uart_send_buff(BYS_UART_PORT, pkt, BYS_PKT_LEN);
}

/* 根据响应命令码更新全局设备状态，返回是否为查询响应（非错误包） */
static uint8 apply_response(uint16 cmd, uint16 data)
{
    switch (cmd) {
        case BYS_RSP_MODE:            g_bys_state.mode            = data; return 1;
        case BYS_RSP_WIRE_DIAMETER:   g_bys_state.wire_diameter   = data; return 1;
        case BYS_RSP_SMART_MODE:      g_bys_state.smart_mode      = data; return 1;
        case BYS_RSP_MIG_CURRENT:     g_bys_state.mig_current     = data; return 1;
        case BYS_RSP_MIG_VOLTAGE_ADJ: g_bys_state.mig_voltage_adj = data; return 1;
        case BYS_RSP_T2T4:            g_bys_state.t2t4            = data; return 1;
        case BYS_RSP_TIG_CURRENT:     g_bys_state.tig_current     = data; return 1;
        case BYS_RSP_MMA_CURRENT:     g_bys_state.mma_current     = data; return 1;
        case BYS_RSP_VRD:             g_bys_state.vrd             = data; return 1;
        case BYS_RSP_MIG_VOLTAGE:     g_bys_state.mig_voltage_actual = data; return 1;
        case BYS_RSP_ALARM:           g_bys_state.alarm           = data; return 1;
        case BYS_RSP_INPUT_VOLTAGE:
            g_bys_state.input_voltage = data;
            g_bys_state.valid         = 1;
            return 1;
        case BYS_RSP_ERROR:
            LOG("[BYS] ERR code=0x%04x\n", data);
            return 1;
        default:
            return 0;
    }
}

/* UART 中断回调：RX搬数据触发OSAL事件；TX完成延时清busy */
static void uart_rx_cb(uart_Evt_t *evt)
{
    if (evt->type == UART_EVT_TYPE_TX_COMPLETED) {
        osal_start_timerEx(s_task_id, s_tx_next_evt, 15);
        return;
    }

    if (evt->type != UART_EVT_TYPE_RX_DATA &&
        evt->type != UART_EVT_TYPE_RX_DATA_TO) return;

    uint8 copy = evt->len;
    if (s_rx_len + copy > sizeof(s_rx_buf))
        copy = (uint8)(sizeof(s_rx_buf) - s_rx_len);

    osal_memcpy(s_rx_buf + s_rx_len, evt->data, copy);
    s_rx_len += copy;
    osal_set_event(s_task_id, s_rx_evt);
}

/* ─── 对外接口 ──────────────────────────────────── */

void bys_uart_init(uint8 task_id, uint16 rx_evt, uint16 tx_next_evt, bys_uart_rx_cb_t rx_cb)
{
    s_task_id     = task_id;
    s_rx_evt      = rx_evt;
    s_tx_next_evt = tx_next_evt;
    s_rx_cb       = rx_cb;

    uart_Cfg_t cfg = {
        .tx_pin      = BYS_UART_TX_PIN,
        .rx_pin      = BYS_UART_RX_PIN,
        .rts_pin     = GPIO_DUMMY,
        .cts_pin     = GPIO_DUMMY,
        .baudrate    = BYS_UART_BAUD,
        .use_fifo    = TRUE,
        .hw_fwctrl   = FALSE,
        .use_tx_buf  = TRUE,
        .parity      = FALSE,
        .evt_handler = uart_rx_cb,
    };
    hal_uart_init(cfg, BYS_UART_PORT);
    hal_uart_set_tx_buf(BYS_UART_PORT, s_tx_hw_buf, BYS_TX_BUF_SIZE);

    hal_pwrmgr_lock(MOD_UART1);
}

/* 发送轮询命令：忙/throttle/队列非空→1，否则发轮询→0（轮询包不入队） */
uint8 bys_uart_poll_next(uint8 app_connected)
{
    if (s_tx_busy) return 1;
    if (s_tx_head != s_tx_tail) { tx_process(); return 1; }
    if (s_tx_throttle) return 1;

    uint16 dev_type = app_connected ? BYS_DEV_APP_ON : BYS_DEV_APP_OFF;
    uint16 cmd     = s_query_cmds[s_query_idx];
    uint16 data    = 0x0000;
    uint16 chksum  = cmd + data;

    s_poll_pkt[0]  = BYS_HEADER_0;
    s_poll_pkt[1]  = BYS_HEADER_1;
    s_poll_pkt[2]  = LO_UINT16(dev_type);
    s_poll_pkt[3]  = HI_UINT16(dev_type);
    s_poll_pkt[4]  = LO_UINT16(cmd);
    s_poll_pkt[5]  = HI_UINT16(cmd);
    s_poll_pkt[6]  = LO_UINT16(data);
    s_poll_pkt[7]  = HI_UINT16(data);
    s_poll_pkt[8]  = LO_UINT16(chksum);
    s_poll_pkt[9]  = HI_UINT16(chksum);
    s_poll_pkt[10] = BYS_TAIL_0;
    s_poll_pkt[11] = BYS_TAIL_1;

    s_query_idx = (s_query_idx + 1) % BYS_QUERY_COUNT;
    s_tx_busy = 1;
    s_tx_throttle = 1;
    hal_uart_send_buff(BYS_UART_PORT, s_poll_pkt, BYS_PKT_LEN);
    return 0;
}

/* 清除统一TX节流标志（定时器到期调用） */
void bys_uart_tick(void)
{
    s_tx_throttle = 0;
}

/* 解析接收缓冲区：校验通过后入通知队列（不再立即回调） */
void bys_uart_process_rx(void)
{
    uint8 i = 0;
    while (i + BYS_PKT_LEN <= s_rx_len) {
        if (s_rx_buf[i]    == BYS_HEADER_0 &&
            s_rx_buf[i+1]  == BYS_HEADER_1 &&
            s_rx_buf[i+10] == BYS_TAIL_0   &&
            s_rx_buf[i+11] == BYS_TAIL_1)
        {
            uint16 cmd    = BUILD_UINT16(s_rx_buf[i+4], s_rx_buf[i+5]);
            uint16 data   = BUILD_UINT16(s_rx_buf[i+6], s_rx_buf[i+7]);
            uint16 chksum = BUILD_UINT16(s_rx_buf[i+8], s_rx_buf[i+9]);

            if (chksum == (uint16)(cmd + data)) {
                g_bys_state.device_type = BUILD_UINT16(s_rx_buf[i+2], s_rx_buf[i+3]);
                apply_response(cmd, data);
                notify_enqueue(s_rx_buf + i);  /* 入通知队列，不立即回调 */
            }
            i += BYS_PKT_LEN;
        } else {
            i++;
        }
    }
    if (i > 0) {
        s_rx_len -= i;
        if (s_rx_len > 0)
            osal_memcpy(s_rx_buf, s_rx_buf + i, s_rx_len);
    }

    tx_process();
}

/* 从通知队列取一包→回调上层（BLE Notify到APP），返回1=已发，0=队列空 */
uint8 bys_uart_notify_process(void)
{
    uint8 *pkt = notify_dequeue();
    if (pkt == NULL) return 0;
    if (s_rx_cb != NULL) s_rx_cb(pkt);
    return 1;
}

/* APP控制指令入队，返回0成功 */
uint8 bys_uart_send_app_cmd(uint8 *buf, uint8 len)
{
    if (len != BYS_PKT_LEN) return 1;
    if (buf[0] != BYS_HEADER_0 || buf[1] != BYS_HEADER_1) return 1;

    buf[2] = LO_UINT16(BYS_DEV_APP_ON);
    buf[3] = HI_UINT16(BYS_DEV_APP_ON);

    if (tx_enqueue(buf) != 0) {
        LOG("[BYS] TX queue full, drop APP cmd\n");
        return 1;
    }

    tx_process();
    return 0;
}

/* TX完成事件中调用：清busy，消费队列。返回0=队列空，1=还有待发 */
uint8 bys_uart_tx_process(void)
{
    s_tx_busy = 0;
    if (s_tx_head == s_tx_tail) return 0;
    tx_process();
    return 1;
}
