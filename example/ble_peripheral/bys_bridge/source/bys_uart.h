#ifndef BYS_UART_H
#define BYS_UART_H

#include "bcomdef.h"

/* ─── 串口硬件配置 ─────────────────────────────── */
#define BYS_UART_PORT       UART1
#define BYS_UART_TX_PIN     P23
#define BYS_UART_RX_PIN     P24
#define BYS_UART_BAUD       19200

/* ─── 协议常量 ──────────────────────────────────── */
#define BYS_PKT_LEN         12
#define BYS_HEADER_0        0xAA
#define BYS_HEADER_1        0x55
#define BYS_TAIL_0          0xBB
#define BYS_TAIL_1          0x55

/* 设备类型字段（上位机发送时填写） */
#define BYS_DEV_APP_ON      0x8000u   /* APP已连接 */
#define BYS_DEV_APP_OFF     0x0000u   /* APP未连接 */

/* 查询命令码 */
#define BYS_CMD_QUERY_MODE      0x0002u
#define BYS_CMD_QUERY_T2T4      0x0003u
#define BYS_CMD_QUERY_CURRENT   0x0004u
#define BYS_CMD_QUERY_POSTGAS   0x0005u
#define BYS_CMD_QUERY_ARC       0x0006u
#define BYS_CMD_QUERY_UNIT      0x0007u
#define BYS_CMD_QUERY_ALARM     0x0008u
#define BYS_CMD_QUERY_VOLTAGE   0x0009u
#define BYS_QUERY_COUNT         8u

/* 响应命令码 */
#define BYS_RSP_ERROR       0x8100u
#define BYS_RSP_MODE        0x0082u
#define BYS_RSP_T2T4        0x0083u
#define BYS_RSP_CURRENT     0x0084u
#define BYS_RSP_POSTGAS     0x0085u
#define BYS_RSP_ARC         0x0086u
#define BYS_RSP_UNIT        0x0087u
#define BYS_RSP_ALARM       0x0088u
#define BYS_RSP_VOLTAGE     0x0089u

/* ─── 设备状态结构体 ─────────────────────────────── */
typedef struct {
    uint16 mode;        /* 0=钢板 1=网格 2=除锈 */
    uint16 t2t4;        /* 0=2T   1=4T */
    uint16 current;     /* 电流(A) */
    uint16 postgas;     /* 后气时间(s) */
    uint16 arc;         /* 维弧时间(s) */
    uint16 unit;        /* 0=PSI 1=MPa 2=BAR */
    uint16 alarm;       /* 0=无 1=过流 2=过热 */
    uint16 voltage;     /* 0=120V 1=240V */
    uint8  valid;       /* 至少完成一轮查询后置1 */
} bys_device_state_t;

extern bys_device_state_t g_bys_state;

/* ─── 回调函数类型 ──────────────────────────────── */
/* 每收到下位机一包完整响应时的回调（raw_pkt 为12字节原始包） */
typedef void (*bys_uart_rx_cb_t)(uint8 *raw_pkt);

/* ─── 接口函数 ──────────────────────────────────── */

/* 初始化UART1，tx_next_evt为TX完成后触发的OSAL事件位 */
void bys_uart_init(uint8 task_id, uint16 rx_evt, uint16 tx_next_evt, bys_uart_rx_cb_t rx_cb);

/* 在BYS_UART_TX_NEXT_EVT事件处理里调用，从队列取下一包发送 */
void bys_uart_tx_process(void);

/* 向下位机发送下一条轮询查询（内部自动循环8条命令），队满返回1 */
uint8 bys_uart_poll_next(uint8 app_connected);

/* 在OSAL的BYS_UART_RX_EVT事件里调用，解析接收缓冲区 */
void bys_uart_process_rx(void);

/* APP控制指令入队：将12字节包加入发送队列（高优先级，立即发送），返回0成功 */
uint8 bys_uart_send_app_cmd(uint8 *buf, uint8 len);

#endif /* BYS_UART_H */
