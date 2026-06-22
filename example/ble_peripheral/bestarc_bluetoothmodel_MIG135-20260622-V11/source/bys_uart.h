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

/* 查询命令码 — MIG145 Pro 协议 */
#define BYS_CMD_QUERY_MODE             0x0002u  /* 查询 MIG/LIFT TIG/MMA 模式 */
#define BYS_CMD_QUERY_WIRE_DIAMETER    0x0003u  /* 查询焊丝直径 */
#define BYS_CMD_QUERY_SMART_MODE       0x0004u  /* 查询智能模式 */
#define BYS_CMD_QUERY_MIG_CURRENT      0x0005u  /* 查询 MIG 电流 */
#define BYS_CMD_QUERY_MIG_VOLTAGE_ADJ  0x0006u  /* 查询 MIG 电压调节 */
#define BYS_CMD_QUERY_T2T4             0x0007u  /* 查询 2T/4T */
#define BYS_CMD_QUERY_TIG_CURRENT      0x0008u  /* 查询 TIG 电流 */
#define BYS_CMD_QUERY_MMA_CURRENT      0x0009u  /* 查询 MMA 电流 */
#define BYS_CMD_QUERY_VRD              0x000Au  /* 查询 VRD 开/关 */
#define BYS_CMD_QUERY_MIG_VOLTAGE      0x000Bu  /* 查询 MIG 实际电压 */
#define BYS_CMD_QUERY_ALARM            0x000Cu  /* 查询报警状态 */
#define BYS_CMD_QUERY_INPUT_VOLTAGE    0x000Du  /* 查询输入电压 */
#define BYS_QUERY_COUNT                12u

/* 响应命令码 */
#define BYS_RSP_ERROR           0x8100u
#define BYS_RSP_MODE            0x0082u  /* 上报 MIG/LIFT TIG/MMA 模式 */
#define BYS_RSP_WIRE_DIAMETER   0x0083u  /* 上报焊丝直径 */
#define BYS_RSP_SMART_MODE      0x0084u  /* 上报智能模式 */
#define BYS_RSP_MIG_CURRENT     0x0085u  /* 上报 MIG 电流 */
#define BYS_RSP_MIG_VOLTAGE_ADJ 0x0086u  /* 上报 MIG 电压调节 */
#define BYS_RSP_T2T4            0x0087u  /* 上报 2T/4T */
#define BYS_RSP_TIG_CURRENT     0x0088u  /* 上报 TIG 电流 */
#define BYS_RSP_MMA_CURRENT     0x0089u  /* 上报 MMA 电流 */
#define BYS_RSP_VRD             0x008Au  /* 上报 VRD 开/关 */
#define BYS_RSP_MIG_VOLTAGE     0x008Bu  /* 上报 MIG 实际电压 */
#define BYS_RSP_ALARM           0x008Cu  /* 上报报警状态 */
#define BYS_RSP_INPUT_VOLTAGE   0x008Du  /* 上报输入电压 */

/* ─── 设备状态结构体 ─────────────────────────────── */
typedef struct {
    uint16 device_type;         /* 设备机型，从下位机响应包中获取 */
    uint16 mode;                /* 0=MIG 1=LIFT TIG 2=MMA */
    uint16 wire_diameter;       /* 0=0.030 1=0.035 2=0.040 */
    uint16 smart_mode;          /* 0=开 1=关 */
    uint16 mig_current;         /* MIG 电流 */
    uint16 mig_voltage_adj;     /* MIG 电压调节 -3.0~3.0V (整数 0~60) */
    uint16 t2t4;                /* 0=2T 1=4T */
    uint16 tig_current;         /* TIG 电流 */
    uint16 mma_current;         /* MMA 电流 */
    uint16 vrd;                 /* 0=关 1=开 */
    uint16 mig_voltage_actual;  /* MIG 实际电压 (整数 135~242, 即 13.5~24.2V) */
    uint16 alarm;               /* 0=无 1=过流 2=过热 */
    uint16 input_voltage;       /* 0=110V 1=220V */
    uint8  valid;               /* 至少完成一轮查询后置1 */
} bys_device_state_t;

extern bys_device_state_t g_bys_state;

/* ─── 回调函数类型 ──────────────────────────────── */
/* 每收到下位机一包完整响应时的回调（raw_pkt 为12字节原始包） */
typedef void (*bys_uart_rx_cb_t)(uint8 *raw_pkt);

/* ─── 接口函数 ──────────────────────────────────── */

/* 初始化UART1，tx_next_evt为TX完成后触发的OSAL事件位 */
void bys_uart_init(uint8 task_id, uint16 rx_evt, uint16 tx_next_evt, bys_uart_rx_cb_t rx_cb);

/* TX完成事件中调用：清busy，消费队列。返回0=队列空，1=还有待发 */
uint8 bys_uart_tx_process(void);

/* 发送下一包轮询：忙/节流/队列非空→1，成功→0（轮询包不入队） */
uint8 bys_uart_poll_next(uint8 app_connected);

/* 清除统一TX节流标志，由定时器到期调用 */
void bys_uart_tick(void);

/* RX事件中调用：解析接收缓冲，校验后入通知队列 */
void bys_uart_process_rx(void);

/* 从通知队列取一包回调上层（BLE Notify），返回1=已发，0=队列空 */
uint8 bys_uart_notify_process(void);

/* APP控制指令入队，返回0成功 */
uint8 bys_uart_send_app_cmd(uint8 *buf, uint8 len);

#endif /* BYS_UART_H */
