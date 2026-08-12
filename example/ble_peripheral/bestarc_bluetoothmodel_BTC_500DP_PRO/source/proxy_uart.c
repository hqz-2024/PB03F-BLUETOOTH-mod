#include "proxy_uart.h"
#include "uart.h"
#include "gpio.h"
#include "OSAL.h"
#include "log.h"
#include "pwrmgr.h"

/* TX 硬件缓冲区（UART0 异步发送用） */
#define PROXY_TX_HW_BUF_SIZE   32
static uint8  s_tx_hw_buf[PROXY_TX_HW_BUF_SIZE];

/* ─── 模块内部变量 ─────────────────────────────── */
static uint8  s_task_id;
static uint16 s_rx_evt;
static uint16 s_hb_timeout_evt;
static uint16 s_tx_next_evt;
static proxy_uart_rx_cb_t        s_rx_cb;
static proxy_heartbeat_wake_cb_t s_hb_wake_cb;

/* 第三方设备在线标志 */
static uint8  s_connected = 0;

/* TX busy：1=硬件正在发送，期间 tx_process() 不会起新的发送 */
static uint8  s_tx_busy = 0;

/*
   TX 活动标志：覆盖整个 TX 窗口（发送 + 回环等待）
   此期间 UART0 RX 收到的所有数据均为 RS485 回环，直接丢弃
*/
static uint8  s_tx_active = 0;

/* RX 环形缓冲区：最多缓存 3 包 */
static uint8  s_rx_buf[PROXY_PKT_LEN * 3];
static uint8  s_rx_len = 0;

/* ─── TX 发送队列：缓存待发数据包，FIFO 不丢包 ──────── */
#define TX_QUEUE_SIZE   3
static uint8  s_tx_queue[TX_QUEUE_SIZE][PROXY_PKT_LEN];
static uint8  s_tx_head = 0;
static uint8  s_tx_tail = 0;

/* ─── 内部辅助 ──────────────────────────────────── */

/* 检测是否为心跳包 */
static uint8 is_heartbeat(const uint8 *pkt)
{
    uint16 cmd    = BUILD_UINT16(pkt[4], pkt[5]);
    uint16 data   = BUILD_UINT16(pkt[6], pkt[7]);
    uint16 chksum = BUILD_UINT16(pkt[8], pkt[9]);
    return (cmd == PROXY_CMD_HEARTBEAT &&
            data == PROXY_DATA_HEARTBEAT &&
            chksum == (uint16)(cmd + data)) ? 1 : 0;
}

/* 入队（队满返回1） */
static uint8 tx_enqueue(uint8 *pkt)
{
    uint8 next_tail = (s_tx_tail + 1) % TX_QUEUE_SIZE;
    if (next_tail == s_tx_head) return 1;

    osal_memcpy(s_tx_queue[s_tx_tail], pkt, PROXY_PKT_LEN);
    s_tx_tail = next_tail;
    return 0;
}

/* 出队（队空返回NULL） */
static uint8 *tx_dequeue(void)
{
    if (s_tx_head == s_tx_tail) return NULL;
    uint8 *pkt = s_tx_queue[s_tx_head];
    s_tx_head = (s_tx_head + 1) % TX_QUEUE_SIZE;
    return pkt;
}

/* 尝试发送队列头部一包；返回1=已发出 0=未发出（busy或队空） */
static uint8 tx_process(void)
{
    if (s_tx_busy) return 0;
    if (!s_connected) {
        /* 设备已断连，清空队列避免积压 */
        s_tx_head = s_tx_tail;
        return 0;
    }
    uint8 *pkt = tx_dequeue();
    if (pkt == NULL) return 0;
    s_tx_busy = 1;
    s_tx_active = 1;  /* 标记 TX 窗口开始，抑制 RS485 回环 */
    hal_uart_send_buff(UART0, pkt, PROXY_PKT_LEN);
    return 1;
}

/* ─── UART0 中断回调 ───────────────────────────── */
static void uart0_rx_cb(uart_Evt_t *evt)
{
    if (evt->type == UART_EVT_TYPE_TX_COMPLETED) {
        /* 保持 busy=1，等 TX_NEXT_EVT 到期后才清零并发下一包 */
        osal_start_timerEx(s_task_id, s_tx_next_evt, PROXY_TX_INTERVAL_MS);
        return;
    }

    if (evt->type != UART_EVT_TYPE_RX_DATA &&
        evt->type != UART_EVT_TYPE_RX_DATA_TO) return;

    /* RS485 半双工回环抑制：TX 窗口内的 RX 数据全部丢弃 */
    if (s_tx_active) return;

    uint8 copy = evt->len;
    if (s_rx_len + copy > sizeof(s_rx_buf))
        copy = (uint8)(sizeof(s_rx_buf) - s_rx_len);

    osal_memcpy(s_rx_buf + s_rx_len, evt->data, copy);
    s_rx_len += copy;
    osal_set_event(s_task_id, s_rx_evt);
}

/* ─── 对外接口 ──────────────────────────────────── */

void proxy_uart_init(uint8 task_id,
                     uint16 rx_evt,
                     uint16 hb_timeout_evt,
                     uint16 tx_next_evt,
                     proxy_uart_rx_cb_t rx_cb,
                     proxy_heartbeat_wake_cb_t hb_wake_cb)
{
    s_task_id       = task_id;
    s_rx_evt        = rx_evt;
    s_hb_timeout_evt = hb_timeout_evt;
    s_tx_next_evt   = tx_next_evt;
    s_rx_cb         = rx_cb;
    s_hb_wake_cb    = hb_wake_cb;

    /* 释放 LOG_INIT 占用的 UART0(P9/P10) */
    hal_uart_deinit(UART0);

    /* 关闭 P9/P10 的 FULLMUX，回退为 GPIO（维持 main.c 中配置的上拉） */
    hal_gpio_fmux(P9,  Bit_DISABLE);
    hal_gpio_fmux(P10, Bit_DISABLE);

    /* 将 UART0 重映射到 P33(TX)/P32(RX) */
    hal_gpio_fmux_set(P33, FMUX_UART0_TX);
    hal_gpio_fmux_set(P32, FMUX_UART0_RX);
    /* hal_gpio_fmux_set 内部已调用 hal_gpio_fmux(pin, Bit_ENABLE) */

    /* 初始化 UART0 */
    uart_Cfg_t cfg = {
        .tx_pin      = PROXY_UART_TX_PIN,
        .rx_pin      = PROXY_UART_RX_PIN,
        .rts_pin     = GPIO_DUMMY,
        .cts_pin     = GPIO_DUMMY,
        .baudrate    = PROXY_UART_BAUD,
        .use_fifo    = TRUE,
        .hw_fwctrl   = FALSE,
        .use_tx_buf  = TRUE,
        .parity      = FALSE,
        .evt_handler = uart0_rx_cb,
    };
    hal_uart_init(cfg, UART0);
    hal_uart_set_tx_buf(UART0, s_tx_hw_buf, PROXY_TX_HW_BUF_SIZE);

    /* 锁定 UART0 电源域，防止时钟门控导致 RX 中断丢失 */
    hal_pwrmgr_lock(MOD_UART0);
}

void proxy_uart_process_rx(void)
{
    uint8 i = 0;
    while (i + PROXY_PKT_LEN <= s_rx_len) {
        if (s_rx_buf[i]    == PROXY_HEADER_0 &&
            s_rx_buf[i+1]  == PROXY_HEADER_1 &&
            s_rx_buf[i+10] == PROXY_TAIL_0   &&
            s_rx_buf[i+11] == PROXY_TAIL_1)
        {
            uint16 cmd    = BUILD_UINT16(s_rx_buf[i+4], s_rx_buf[i+5]);
            uint16 data   = BUILD_UINT16(s_rx_buf[i+6], s_rx_buf[i+7]);
            uint16 chksum = BUILD_UINT16(s_rx_buf[i+8], s_rx_buf[i+9]);

            if (chksum == (uint16)(cmd + data)) {
                if (is_heartbeat(s_rx_buf + i)) {
                    /* 心跳包：首次上线唤醒，后续刷新超时定时器 */
                    if (!s_connected) {
                        s_connected = 1;
                        if (s_hb_wake_cb != NULL) {
                            s_hb_wake_cb();
                        }
                    }
                    osal_start_timerEx(s_task_id, s_hb_timeout_evt,
                                       PROXY_HEARTBEAT_TIMEOUT_MS);
                } else {
                    /* 非心跳包：透传给上层处理（转发到主控） */
                    if (s_rx_cb != NULL) {
                        s_rx_cb(s_rx_buf + i);
                    }
                }
            }
            i += PROXY_PKT_LEN;
        } else {
            i++;
        }
    }

    /* 移除已处理的字节 */
    if (i > 0) {
        s_rx_len -= i;
        if (s_rx_len > 0)
            osal_memcpy(s_rx_buf, s_rx_buf + i, s_rx_len);
    }
}

uint8 proxy_uart_send(uint8 *pkt, uint8 len)
{
    if (len != PROXY_PKT_LEN) return 1;
    if (!s_connected)         return 1;

    /* 入队（队满丢弃） */
    if (tx_enqueue(pkt) != 0) {
        LOG("[PROXY] TX queue full, drop pkt\n");
        return 1;
    }

    /* 立即尝试发送 */
    tx_process();
    return 0;
}

void proxy_uart_tx_process(void)
{
    s_tx_busy = 0;
    s_tx_active = 0;  /* TX 窗口结束，恢复 RX 接收 */
    tx_process();
}

uint8 proxy_uart_is_connected(void)
{
    return s_connected;
}

/* 清空 TX 队列中的积压数据（当前正在发送的一包不受影响） */
void proxy_uart_flush_tx(void)
{
    s_tx_head = s_tx_tail;
}

uint8 proxy_uart_handle_timeout(void)
{
    if (s_connected) {
        s_connected = 0;
        /* 断连时清空 TX 队列，避免积压旧数据 */
        s_tx_head = s_tx_tail;
        return 1;  /* 刚刚断连 */
    }
    return 0;  /* 已是断连态 */
}
