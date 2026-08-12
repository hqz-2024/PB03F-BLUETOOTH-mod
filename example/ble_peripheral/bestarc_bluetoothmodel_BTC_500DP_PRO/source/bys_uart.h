#ifndef BYS_UART_H
#define BYS_UART_H

#include "bcomdef.h"

/* ─── 串口硬件配置 ─────────────────────────────── */
#define BYS_UART_PORT       UART1
#define BYS_UART_TX_PIN     P24
#define BYS_UART_RX_PIN     P23
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

/* OTA 触发命令码（双向，设备类型字段忽略） */
#define BYS_CMD_OTA_TRIGGER     0xFE00u
#define BYS_DATA_OTA_TRIGGER    0x00FEu

/* ─── 设备型号（主控响应帧 2-3 位，低字节） ──────── */
#define BYS_DEV_TYPE_BTC500DP_PRO   0x0002u   /* BTC 系列 */
#define BYS_DEV_TYPE_BTC550DP_ULTRA 0x0003u   /* BTC 系列 */
#define BYS_DEV_TYPE_BTC500DP_7GEN  0x0004u   /* BTC 系列 */
#define BYS_DEV_TYPE_BTC500DP_5GEN  0x0005u   /* BTC 系列（单功能机，实测兼容8条轮询） */
#define BYS_DEV_TYPE_MIG145         0x0006u   /* MIG 系列 */
#define BYS_DEV_TYPE_MIG135         0x0007u   /* MIG 系列 */

/* 设备是否属于 MIG 系列（轮询规则/广播布局按系列区分） */
#define IS_MIG_SERIES(dt)  ((dt) == BYS_DEV_TYPE_MIG145 || (dt) == BYS_DEV_TYPE_MIG135)

/* 查询命令码（BTC 系列，8 条） */
#define BYS_CMD_QUERY_MODE      0x0002u
#define BYS_CMD_QUERY_T2T4      0x0003u
#define BYS_CMD_QUERY_CURRENT   0x0004u
#define BYS_CMD_QUERY_POSTGAS   0x0005u
#define BYS_CMD_QUERY_ARC       0x0006u
#define BYS_CMD_QUERY_UNIT      0x0007u
#define BYS_CMD_QUERY_ALARM     0x0008u
#define BYS_CMD_QUERY_VOLTAGE   0x0009u
#define BYS_QUERY_COUNT_BTC     8u

/* 查询命令码（MIG 系列扩展，0x0002-0x000D 共12条） */
#define BYS_CMD_MIG_QUERY_VOLTADJ   0x000Au   /* MIG电压调整(-3.0~3.0) */
#define BYS_CMD_MIG_QUERY_VOLTAGE2  0x000Bu   /* MIG电压显示(13.5~24.2) */
#define BYS_CMD_MIG_QUERY_ALARM     0x000Cu
#define BYS_CMD_MIG_QUERY_VOLTAGE   0x000Du
#define BYS_QUERY_COUNT_MIG     12u

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

/* 响应命令码（MIG 系列，广播所需字段） */
#define BYS_RSP_MIG_WIRE      0x0083u   /* 焊丝直径 */
#define BYS_RSP_MIG_CURRENT   0x0085u   /* MIG电流 */
#define BYS_RSP_MIG_T2T4      0x0087u
#define BYS_RSP_MIG_ALARM     0x008Cu
#define BYS_RSP_MIG_VOLTAGE   0x008Du

/* ─── 设备状态结构体 ─────────────────────────────── */
typedef struct {
    uint16 device_type; /* 设备机型，从下位机响应包中获取 */
    uint16 mode;        /* BTC: 0=钢板 1=网格 2=除锈；MIG: 0=MIG 1=LIFT TIG 2=MMA */
    uint16 t2t4;        /* 0=2T   1=4T */
    uint16 current;     /* BTC: 电流(A)；MIG: MIG电流 */
    uint16 postgas;     /* 后气时间(s)（BTC） */
    uint16 arc;         /* 维弧时间(s)（BTC） */
    uint16 unit;        /* 0=PSI 1=MPa 2=BAR（BTC） */
    uint16 alarm;       /* 0=无 1=过流 2=过热 */
    uint16 voltage;     /* BTC: 0=120V 1=240V；MIG: 0=110V 1=220V */
    uint16 wire_dia;    /* 焊丝直径（MIG，0x0083响应） */
    uint16 smart;       /* 智能模式开/关（MIG，0x0084响应） */
    uint8  valid;       /* 至少完成一轮查询后置1 */
} bys_device_state_t;

extern bys_device_state_t g_bys_state;

/* ─── 回调函数类型 ──────────────────────────────── */
/* 每收到下位机一包完整响应时的回调（raw_pkt 为12字节原始包） */
typedef void (*bys_uart_rx_cb_t)(uint8 *raw_pkt);

/* ─── 接口函数 ──────────────────────────────────── */

/* 初始化UART1，tx_next_evt为TX完成后触发的OSAL事件位 */
void bys_uart_init(uint8 task_id, uint16 rx_evt, uint16 tx_next_evt, bys_uart_rx_cb_t rx_cb);

/* 在BYS_UART_TX_NEXT_EVT事件处理里调用，清busy并尝试发送队列下一包；返回1=已从队列发出新包，0=队列空闲 */
uint8 bys_uart_tx_process(void);

/* 发送下一条轮询查询：busy返回1；队列非空则优先发队列(返回1)；否则直接发轮询(返回0)，不入队 */
uint8 bys_uart_poll_next(uint8 app_connected);

/* 在OSAL的BYS_UART_RX_EVT事件里调用，解析接收缓冲区 */
void bys_uart_process_rx(void);

/* APP控制指令入队：将12字节包加入发送队列（高优先级，立即发送），返回0成功 */
uint8 bys_uart_send_app_cmd(uint8 *buf, uint8 len);

/* 返回当前生效的查询条数（BTC=8 / MIG=12，由设备型号自动切换） */
uint8 bys_uart_get_query_count(void);

#endif /* BYS_UART_H */
