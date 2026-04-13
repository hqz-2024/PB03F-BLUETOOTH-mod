#include "bys_uart.h"
#include "uart.h"
#include "OSAL.h"
#include "log.h"

/* ─── 模块内部变量 ─────────────────────────────── */
static uint8  s_task_id;
static uint16 s_rx_evt;
static bys_uart_rx_cb_t s_rx_cb;  /* 每包响应回调 */

/* 接收环形缓冲区，最多缓存3包 */
static uint8  s_rx_buf[BYS_PKT_LEN * 3];
static uint8  s_rx_len = 0;

/* 轮询状态：当前待发送的查询命令索引 */
static uint8  s_query_idx = 0;

/* 发送队列：最多缓存2包APP控制指令（高优先级） + 1包轮询查询 */
#define TX_QUEUE_SIZE   3
static uint8  s_tx_queue[TX_QUEUE_SIZE][BYS_PKT_LEN];
static uint8  s_tx_head = 0;  /* 队头（读位置） */
static uint8  s_tx_tail = 0;  /* 队尾（写位置） */
static uint8  s_tx_busy = 0;  /* 当前是否正在发送 */

/* 全局设备状态，供广播数据使用 */
bys_device_state_t g_bys_state = {0};

/* 8条查询命令循环表 */
static const uint16 s_query_cmds[BYS_QUERY_COUNT] = {
    BYS_CMD_QUERY_MODE,
    BYS_CMD_QUERY_T2T4,
    BYS_CMD_QUERY_CURRENT,
    BYS_CMD_QUERY_POSTGAS,
    BYS_CMD_QUERY_ARC,
    BYS_CMD_QUERY_UNIT,
    BYS_CMD_QUERY_ALARM,
    BYS_CMD_QUERY_VOLTAGE,
};

/* ─── 内部函数 ──────────────────────────────────── */

/* 发送队列入队（队满返回1） */
static uint8 tx_enqueue(uint8 *pkt)
{
    uint8 next_tail = (s_tx_tail + 1) % TX_QUEUE_SIZE;
    if (next_tail == s_tx_head) return 1;  /* 队满 */

    osal_memcpy(s_tx_queue[s_tx_tail], pkt, BYS_PKT_LEN);
    s_tx_tail = next_tail;
    return 0;
}

/* 发送队列出队（队空返回NULL） */
static uint8* tx_dequeue(void)
{
    if (s_tx_head == s_tx_tail) return NULL;  /* 队空 */
    uint8 *pkt = s_tx_queue[s_tx_head];
    s_tx_head = (s_tx_head + 1) % TX_QUEUE_SIZE;
    return pkt;
}

/* 尝试发送队列头部的一包（无数据则返回） */
static void tx_process(void)
{
    if (s_tx_busy) return;

    uint8 *pkt = tx_dequeue();
    if (pkt == NULL) return;

    s_tx_busy = 1;
    hal_uart_send_buff(BYS_UART_PORT, pkt, BYS_PKT_LEN);
    s_tx_busy = 0;  /* 同步发送，立即清除 */
}

/* 构造并发送一个标准12字节数据包（加入发送队列） */
static void send_packet(uint16 dev_type, uint16 cmd, uint16 data)
{
    uint8  pkt[BYS_PKT_LEN];
    uint16 chksum = cmd + data;

    pkt[0]  = BYS_HEADER_0;
    pkt[1]  = BYS_HEADER_1;
    pkt[2]  = LO_UINT16(dev_type);
    pkt[3]  = HI_UINT16(dev_type);
    pkt[4]  = LO_UINT16(cmd);
    pkt[5]  = HI_UINT16(cmd);
    pkt[6]  = LO_UINT16(data);
    pkt[7]  = HI_UINT16(data);
    pkt[8]  = LO_UINT16(chksum);
    pkt[9]  = HI_UINT16(chksum);
    pkt[10] = BYS_TAIL_0;
    pkt[11] = BYS_TAIL_1;

    if (tx_enqueue(pkt) == 0) {
        tx_process();  /* 入队成功，立即尝试发送 */
    }
}

/* 根据响应命令码更新全局设备状态，返回是否为查询响应（非错误包） */
static uint8 apply_response(uint16 cmd, uint16 data)
{
    switch (cmd) {
        case BYS_RSP_MODE:    g_bys_state.mode    = data; return 1;
        case BYS_RSP_T2T4:    g_bys_state.t2t4    = data; return 1;
        case BYS_RSP_CURRENT: g_bys_state.current = data; return 1;
        case BYS_RSP_POSTGAS: g_bys_state.postgas = data; return 1;
        case BYS_RSP_ARC:     g_bys_state.arc     = data; return 1;
        case BYS_RSP_UNIT:    g_bys_state.unit    = data; return 1;
        case BYS_RSP_ALARM:   g_bys_state.alarm   = data; return 1;
        case BYS_RSP_VOLTAGE:
            g_bys_state.voltage = data;
            g_bys_state.valid   = 1;   /* 一轮完成 */
            return 1;
        case BYS_RSP_ERROR:
            LOG("[BYS] ERR code=0x%04x\n", data);
            return 1;  /* 错误包也是响应 */
        default:
            return 0;  /* 未知命令，可能是APP控制指令的确认包(0x82XX) */
    }
}

/* UART RX 中断回调，在中断上下文中执行，仅搬数据+触发OSAL事件 */
static void uart_rx_cb(uart_Evt_t *evt)
{
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

void bys_uart_init(uint8 task_id, uint16 rx_evt, bys_uart_rx_cb_t rx_cb)
{
    s_task_id = task_id;
    s_rx_evt  = rx_evt;
    s_rx_cb   = rx_cb;

    uart_Cfg_t cfg = {
        .tx_pin      = BYS_UART_TX_PIN,
        .rx_pin      = BYS_UART_RX_PIN,
        .rts_pin     = GPIO_DUMMY,
        .cts_pin     = GPIO_DUMMY,
        .baudrate    = BYS_UART_BAUD,
        .use_fifo    = TRUE,
        .hw_fwctrl   = FALSE,
        .use_tx_buf  = FALSE,
        .parity      = FALSE,
        .evt_handler = uart_rx_cb,
    };
    hal_uart_init(cfg, BYS_UART_PORT);
}

/* 发送当前轮询命令，内部自动推进索引，返回0成功 */
uint8 bys_uart_poll_next(uint8 app_connected)
{
    uint16 dev_type = app_connected ? BYS_DEV_APP_ON : BYS_DEV_APP_OFF;
    uint8  pkt[BYS_PKT_LEN];
    uint16 cmd   = s_query_cmds[s_query_idx];
    uint16 data  = 0x0000;
    uint16 chksum = cmd + data;

    pkt[0]  = BYS_HEADER_0;
    pkt[1]  = BYS_HEADER_1;
    pkt[2]  = LO_UINT16(dev_type);
    pkt[3]  = HI_UINT16(dev_type);
    pkt[4]  = LO_UINT16(cmd);
    pkt[5]  = HI_UINT16(cmd);
    pkt[6]  = LO_UINT16(data);
    pkt[7]  = HI_UINT16(data);
    pkt[8]  = LO_UINT16(chksum);
    pkt[9]  = HI_UINT16(chksum);
    pkt[10] = BYS_TAIL_0;
    pkt[11] = BYS_TAIL_1;

    /* 入队（队满则暂停轮询，下个定时器周期重试） */
    if (tx_enqueue(pkt) != 0) {
        return 1;  /* 队满 */
    }

    s_query_idx = (s_query_idx + 1) % BYS_QUERY_COUNT;
    tx_process();  /* 立即尝试发送 */
    return 0;
}

/* 解析接收缓冲区中所有完整数据包，每包立即回调上层 */
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
                /* 更新全局状态（查询响应） */
                apply_response(cmd, data);

                /* 立即回调上层，透传原始12字节包给APP */
                if (s_rx_cb != NULL) {
                    s_rx_cb(s_rx_buf + i);
                }
            }
            i += BYS_PKT_LEN;
        } else {
            i++;  /* 丢弃无效字节，继续寻找包头 */
        }
    }
    /* 移除已处理的字节 */
    if (i > 0) {
        s_rx_len -= i;
        if (s_rx_len > 0)
            osal_memcpy(s_rx_buf, s_rx_buf + i, s_rx_len);
    }

    /* 处理完后尝试发送队列中的下一包 */
    tx_process();
}

/* APP控制指令入队（高优先级），返回0成功 */
uint8 bys_uart_send_app_cmd(uint8 *buf, uint8 len)
{
    if (len != BYS_PKT_LEN) return 1;
    if (buf[0] != BYS_HEADER_0 || buf[1] != BYS_HEADER_1) return 1;

    /* 修正设备类型字段为APP已连接（小端序：低字节在pkt[2]） */
    buf[2] = LO_UINT16(BYS_DEV_APP_ON);
    buf[3] = HI_UINT16(BYS_DEV_APP_ON);

    /* 入队（队满会丢弃） */
    if (tx_enqueue(buf) != 0) {
        LOG("[BYS] TX queue full, drop APP cmd\n");
        return 1;
    }

    /* 立即尝试发送 */
    tx_process();
    return 0;
}
