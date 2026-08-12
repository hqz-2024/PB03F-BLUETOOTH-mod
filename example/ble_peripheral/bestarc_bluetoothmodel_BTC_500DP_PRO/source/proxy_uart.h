#ifndef PROXY_UART_H
#define PROXY_UART_H

#include "bcomdef.h"

/* ─── 串口硬件配置 ─────────────────────────────── */
#define PROXY_UART_PORT       UART0
#define PROXY_UART_TX_PIN     P33
#define PROXY_UART_RX_PIN     P32
#define PROXY_UART_BAUD       19200

/* ─── 心跳超时(ms)：5s 未收到心跳帧才判定断连 ───── */
#define PROXY_HEARTBEAT_TIMEOUT_MS  5000u

/* TX 包间间隔(ms)：每包发完后等此时间再发下一包 */
#define PROXY_TX_INTERVAL_MS        10u

/* ─── 协议常量（与BYS协议格式一致） ────────────── */
#define PROXY_PKT_LEN         12
#define PROXY_HEADER_0        0xAA
#define PROXY_HEADER_1        0x55
#define PROXY_TAIL_0          0xBB
#define PROXY_TAIL_1          0x55

/* 心跳包命令码与数据 */
#define PROXY_CMD_HEARTBEAT   0xF100u
#define PROXY_DATA_HEARTBEAT  0x00F1u

/* ─── 回调函数类型 ──────────────────────────────── */
/*
   收到完整数据包回调（非心跳包的普通指令包）
   由 bys_bridge.c 注册，内部转发至 UART1 主控
*/
typedef void (*proxy_uart_rx_cb_t)(uint8 *raw_pkt);

/*
   心跳唤醒回调：首次收到心跳或断连后重新收到心跳时触发
   由 bys_bridge.c 注册，用于启动轮询
*/
typedef void (*proxy_heartbeat_wake_cb_t)(void);

/* ─── 接口函数 ──────────────────────────────────── */

/*
   初始化 UART0：
   - 释放 LOG_INIT 占用的 P9/P10
   - 将 UART0 重映射到 P33(TX)/P32(RX)
   - 注册 RX 数据回调和心跳唤醒回调
   - 锁定 MOD_UART0 电源域防止时钟门控丢中断
*/
void proxy_uart_init(uint8 task_id,
                     uint16 rx_evt,
                     uint16 hb_timeout_evt,
                     uint16 tx_next_evt,
                     proxy_uart_rx_cb_t rx_cb,
                     proxy_heartbeat_wake_cb_t hb_wake_cb);

/* 在 OSAL 的 RX 事件中调用，解析接收缓冲区中的完整数据包 */
void proxy_uart_process_rx(void);

/* 发送一包数据给第三方设备（加入发送队列，不阻塞） */
uint8 proxy_uart_send(uint8 *pkt, uint8 len);

/* TX_NEXT_EVT 到期后调用：清 busy 并发队列下一包 */
void proxy_uart_tx_process(void);

/* 返回第三方设备是否在线（有心跳） */
uint8 proxy_uart_is_connected(void);

/* 心跳超时事件处理：清除连接状态，返回0=已是断连态 1=刚才断连 */
uint8 proxy_uart_handle_timeout(void);

/* 清空 TX 队列中的积压数据（C 发指令前调用，保证回复干净） */
void proxy_uart_flush_tx(void);

#endif /* PROXY_UART_H */
