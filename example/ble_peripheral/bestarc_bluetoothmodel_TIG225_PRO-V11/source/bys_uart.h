#ifndef BYS_UART_H
#define BYS_UART_H

#include "bcomdef.h"

/* ─── 串口硬件配置 ─────────────────────────────── */
#define BYS_UART_PORT       UART1
#define BYS_UART_TX_PIN     P24
#define BYS_UART_RX_PIN     P23
#define BYS_UART_BAUD       57600  /* ACDC TIG225 Pro 协议 v000：波特率 115200 8N0
                                     * （TIG215 Pro 为 57600，TIG225 Pro 提升为 115200）
                                     * 调试日志走 UART0（P9/P10），波特率不受此项影响，
                                     * 仍由 SDK 的 dbg_printf_init() 决定（115200）。 */

/* ─── 协议常量 ──────────────────────────────────── */
#define BYS_PKT_LEN         12
#define BYS_HEADER_0        0xAA    /* 包头 0x55AA，小端发送 AA 55 */
#define BYS_HEADER_1        0x55
#define BYS_TAIL_0          0xBB    /* 包尾 0x55BB，小端发送 BB 55 */
#define BYS_TAIL_1          0x55

/* 设备类型字段（上位机发送时填写） */
#define BYS_DEV_APP_ON      0x8000u   /* APP已连接 */
#define BYS_DEV_APP_OFF     0x0000u   /* APP未连接 */

/* OTA 触发命令码（双向，设备类型字段忽略） */
#define BYS_CMD_OTA_TRIGGER     0xFE00u
#define BYS_DATA_OTA_TRIGGER    0x00FEu

/* 本机机型码：协议规定设备上报时低字节为机型代码，TIG215 Pro/0x0009，
 * 后续机型以此累加（TIG225 Pro 文档未给出新值，仍沿用 0x0009）。
 * 实际取值以主控上报为准：解析响应包时透传，不在此硬编码。 */
#define BYS_DEV_TYPE_TIG_PRO    0x0009u

/* ─── 命令码：上位机设定（APP下发→主控，本模块只透传不解析）───
 * 0x0200 模式(0=STICK 1=TIG(DC) 2=TIG(AC方波) 3=TIG(AC三角波))
 * 0x0300 热引弧电流(0~100A，0=OFF)     0x0400 推力电流(0~100A，0=OFF)
 * 0x0500 VRD(0=关 1=开)                0x0600 焊接电流
 * 0x0700 2T/4T/点焊(0=2T 1=4T 2=点焊)  0x0800 脉冲(0=关 1=开)
 * 0x0900 点焊时间(1~100 → 0.01~1.00S)  0x0A00 前气时间(1~30 → 0.1~3.0S)
 * 0x0B00 起弧电流                      0x0C00 缓升时间(0~15S)
 * 0x0D00 脉冲占空比(10%~90%)           0x0E00 脉冲频率(5~2000 → 0.5~200.0HZ)
 * 0x0F00 脉冲谷值(5%~95%)              0x1000 AC频率(40~200HZ)
 * 0x1100 AC占空比(20%~60%)             0x1200 缓降时间(0~25S)
 * 0x1300 收弧电流                      0x1400 后气时间(0~150 → 0.0~15.0S)
 */

/* ─── 查询命令码（主控不主动上报，由本模块轮询）─── */
#define BYS_CMD_QUERY_MODE          0x0002u  /* 模式 STICK/TIG(DC)/TIG(AC方波)/TIG(AC三角波) */
#define BYS_CMD_QUERY_HOT_START     0x0003u  /* 热引弧电流 */
#define BYS_CMD_QUERY_THRUST        0x0004u  /* 推力电流 */
#define BYS_CMD_QUERY_VRD           0x0005u  /* VRD 开/关 */
#define BYS_CMD_QUERY_WELD_CURRENT  0x0006u  /* 焊接电流 */
#define BYS_CMD_QUERY_T2T4_SPOT     0x0007u  /* 2T/4T/点焊 */
#define BYS_CMD_QUERY_PULSE         0x0008u  /* 脉冲 开/关 */
#define BYS_CMD_QUERY_SPOT_TIME     0x0009u  /* 点焊时间 */
#define BYS_CMD_QUERY_PRE_GAS       0x000Au  /* 前气时间 */
#define BYS_CMD_QUERY_START_CURRENT 0x000Bu  /* 起弧电流 */
#define BYS_CMD_QUERY_UP_SLOPE      0x000Cu  /* 缓升时间 */
#define BYS_CMD_QUERY_PULSE_DUTY    0x000Du  /* 脉冲占空比 */
#define BYS_CMD_QUERY_PULSE_FREQ    0x000Eu  /* 脉冲频率 */
#define BYS_CMD_QUERY_PULSE_VALLEY  0x000Fu  /* 脉冲谷值 */
#define BYS_CMD_QUERY_AC_FREQ       0x0010u  /* AC频率 */
#define BYS_CMD_QUERY_AC_DUTY       0x0011u  /* AC占空比（反向电流） */
#define BYS_CMD_QUERY_DOWN_SLOPE    0x0012u  /* 缓降时间 */
#define BYS_CMD_QUERY_END_CURRENT   0x0013u  /* 收弧电流 */
#define BYS_CMD_QUERY_POST_GAS      0x0014u  /* 后气时间 */
#define BYS_CMD_QUERY_ALARM         0x0015u  /* 报警 */
#define BYS_CMD_QUERY_INPUT_VOLTAGE 0x0016u  /* 输入电压 */
#define BYS_QUERY_COUNT             21u

/* ─── 响应命令码（0x0082~0x0096 为设备上报查询响应）─── */
#define BYS_RSP_ERROR           0x8100u
#define BYS_RSP_MODE            0x0082u
#define BYS_RSP_HOT_START       0x0083u
#define BYS_RSP_THRUST          0x0084u
#define BYS_RSP_VRD             0x0085u
#define BYS_RSP_WELD_CURRENT    0x0086u
#define BYS_RSP_T2T4_SPOT       0x0087u
#define BYS_RSP_PULSE           0x0088u
#define BYS_RSP_SPOT_TIME       0x0089u
#define BYS_RSP_PRE_GAS         0x008Au
#define BYS_RSP_START_CURRENT   0x008Bu
#define BYS_RSP_UP_SLOPE        0x008Cu
#define BYS_RSP_PULSE_DUTY      0x008Du
#define BYS_RSP_PULSE_FREQ      0x008Eu
#define BYS_RSP_PULSE_VALLEY    0x008Fu
#define BYS_RSP_AC_FREQ         0x0090u
#define BYS_RSP_AC_DUTY         0x0091u
#define BYS_RSP_DOWN_SLOPE      0x0092u
#define BYS_RSP_END_CURRENT     0x0093u
#define BYS_RSP_POST_GAS        0x0094u
#define BYS_RSP_ALARM           0x0095u
#define BYS_RSP_INPUT_VOLTAGE   0x0096u

/* ─── 设备状态结构体 ─────────────────────────────── */
typedef struct {
    uint16 device_type;    /* 设备机型：主控上报值（TIG 系列文档为 0x0009） */
    uint16 mode;           /* 0=STICK 1=TIG(DC) 2=TIG(AC方波) 3=TIG(AC三角波) */
    uint16 hot_start;      /* 热引弧电流 0~100A，0=OFF */
    uint16 thrust;         /* 推力电流   0~100A，0=OFF */
    uint16 vrd;            /* 0=VRD关 1=VRD开 */
    uint16 weld_current;   /* 焊接电流(A) */
    uint16 t2t4_spot;      /* 0=2T 1=4T 2=点焊 */
    uint16 pulse;          /* 0=脉冲关 1=脉冲开 */
    uint16 spot_time;      /* 点焊时间 1~100 (0.01~1.00S) */
    uint16 pre_gas;        /* 前气时间 1~30 (0.1~3.0S) */
    uint16 start_current;  /* 起弧电流(A) */
    uint16 up_slope;       /* 缓升时间 0~15S */
    uint16 pulse_duty;     /* 脉冲占空比 10~90 (%) */
    uint16 pulse_freq;     /* 脉冲频率 5~2000 (0.5~200.0HZ) */
    uint16 pulse_valley;   /* 脉冲谷值 5~95 (%) */
    uint16 ac_freq;        /* AC频率 40~200 (HZ) */
    uint16 ac_duty;        /* AC占空比 20~60 (%) */
    uint16 down_slope;     /* 缓降时间 0~25S */
    uint16 end_current;    /* 收弧电流(A) */
    uint16 post_gas;       /* 后气时间 0~150 (0.0~15.0S) */
    uint16 alarm;          /* 0=无报警 1=过流 */
    uint16 input_voltage;  /* 0=110V 1=220V */
    uint8  valid;          /* 至少完成一轮查询后置1 */
} bys_device_state_t;

extern bys_device_state_t g_bys_state;

/* ─── 回调函数类型 ──────────────────────────────── */
/* 每收到下位机一包完整响应时的回调（raw_pkt 为12字节原始包） */
typedef void (*bys_uart_rx_cb_t)(uint8 *raw_pkt);

/* ─── 接口函数 ──────────────────────────────────── */

/* 初始化UART1，tx_next_evt为TX完成后触发的OSAL事件位 */
void bys_uart_init(uint8 task_id, uint16 rx_evt, uint16 tx_next_evt, bys_uart_rx_cb_t rx_cb);

/* 在BYS_UART_TX_NEXT_EVT事件处理里调用：清busy并发队列下一包，返回1=已发/还有，0=队列空闲 */
uint8 bys_uart_tx_process(void);

/* 向下位机发送下一条轮询查询：忙或队列非空时不发（轮询包不入队），返回1=未发，0=已发 */
uint8 bys_uart_poll_next(uint8 app_connected);

/* 在OSAL的BYS_UART_RX_EVT事件里调用，解析接收缓冲区 */
void bys_uart_process_rx(void);

/* APP控制指令入队：将12字节包加入发送队列（高优先级，立即发送），返回0成功 */
uint8 bys_uart_send_app_cmd(uint8 *buf, uint8 len);

#endif /* BYS_UART_H */
