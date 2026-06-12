#ifndef BYS_UART_H
#define BYS_UART_H

#include "bcomdef.h"
#include "demo_test_config.h"

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

/* 查询命令码 */
#if (DEMO_TEST_PROTOCOL_VARIANT == DEMO_TEST_PROTO_5GEN)
#define BYS_CMD_QUERY_NOP0      0x0002u
#define BYS_CMD_QUERY_T2T4      0x0003u
#define BYS_CMD_QUERY_CURRENT   0x0004u
#define BYS_CMD_QUERY_POSTGAS   0x0005u
#define BYS_CMD_QUERY_NOP1      0x0006u
#define BYS_CMD_QUERY_UNIT      0x0007u
#define BYS_CMD_QUERY_ALARM     0x0008u
#define BYS_CMD_QUERY_VOLTAGE   0x0009u
#else
#define BYS_CMD_QUERY_MODE      0x0002u
#define BYS_CMD_QUERY_T2T4      0x0003u
#define BYS_CMD_QUERY_CURRENT   0x0004u
#define BYS_CMD_QUERY_POSTGAS   0x0005u
#define BYS_CMD_QUERY_ARC       0x0006u
#define BYS_CMD_QUERY_UNIT      0x0007u
#define BYS_CMD_QUERY_ALARM     0x0008u
#define BYS_CMD_QUERY_VOLTAGE   0x0009u
#endif

#define BYS_QUERY_ITEM_MODE       DEMO_TEST_SUPPORT_MODE
#define BYS_QUERY_ITEM_T2T4       DEMO_TEST_SUPPORT_T2T4
#define BYS_QUERY_ITEM_CURRENT    DEMO_TEST_SUPPORT_CURRENT
#define BYS_QUERY_ITEM_POSTGAS    DEMO_TEST_SUPPORT_POSTGAS
#define BYS_QUERY_ITEM_ARC        DEMO_TEST_SUPPORT_ARC
#define BYS_QUERY_ITEM_UNIT       DEMO_TEST_SUPPORT_UNIT
#define BYS_QUERY_ITEM_ALARM      DEMO_TEST_SUPPORT_ALARM
#define BYS_QUERY_ITEM_VOLTAGE    DEMO_TEST_SUPPORT_VOLTAGE

#if (DEMO_TEST_PROTOCOL_VARIANT == DEMO_TEST_PROTO_5GEN)
#define BYS_QUERY_NOP_COUNT        2
#else
#define BYS_QUERY_NOP_COUNT        0
#endif

#define BYS_QUERY_COUNT  ( \
    BYS_QUERY_NOP_COUNT + \
    BYS_QUERY_ITEM_MODE + \
    BYS_QUERY_ITEM_T2T4 + \
    BYS_QUERY_ITEM_CURRENT + \
    BYS_QUERY_ITEM_POSTGAS + \
    BYS_QUERY_ITEM_ARC + \
    BYS_QUERY_ITEM_UNIT + \
    BYS_QUERY_ITEM_ALARM + \
    BYS_QUERY_ITEM_VOLTAGE )

#if (BYS_QUERY_COUNT == 0)
#error "BYS_QUERY_COUNT must be greater than 0"
#endif

/* 查询响应码 */
#define BYS_RSP_ERROR       0x8100u
#if (DEMO_TEST_PROTOCOL_VARIANT == DEMO_TEST_PROTO_5GEN)
#define BYS_RSP_NOP0        0x0082u
#define BYS_RSP_T2T4        0x0083u
#define BYS_RSP_CURRENT     0x0084u
#define BYS_RSP_POSTGAS     0x0085u
#define BYS_RSP_NOP1        0x0086u
#define BYS_RSP_UNIT        0x0087u
#define BYS_RSP_ALARM       0x0088u
#define BYS_RSP_VOLTAGE     0x0089u
#else
#define BYS_RSP_MODE        0x0082u
#define BYS_RSP_T2T4        0x0083u
#define BYS_RSP_CURRENT     0x0084u
#define BYS_RSP_POSTGAS     0x0085u
#define BYS_RSP_ARC         0x0086u
#define BYS_RSP_UNIT        0x0087u
#define BYS_RSP_ALARM       0x0088u
#define BYS_RSP_VOLTAGE     0x0089u
#endif

/* 设置响应码（App→设备设置命令的确认） */
#if (DEMO_TEST_PROTOCOL_VARIANT == DEMO_TEST_PROTO_5GEN)
#define BYS_RSP_SET_T2T4    0x8300u
#define BYS_RSP_SET_CURRENT 0x8400u
#define BYS_RSP_SET_POSTGAS 0x8500u
#define BYS_RSP_SET_UNIT    0x8700u
#else
#define BYS_RSP_SET_MODE    0x8200u
#define BYS_RSP_SET_T2T4    0x8300u
#define BYS_RSP_SET_CURRENT 0x8400u
#define BYS_RSP_SET_POSTGAS 0x8500u
#define BYS_RSP_SET_ARC     0x8600u
#define BYS_RSP_SET_UNIT    0x8700u
#endif

/* ─── 设备状态结构体 ─────────────────────────────── */
typedef struct {
    uint16 device_type; /* 设备机型 */
    uint16 mode;        /* 0=钢板 1=网格 2=除锈 3=气刨 */
    uint16 t2t4;        /* 0=2T   1=4T */
    uint16 current;     /* 电流(A) 15~50 */
    uint16 postgas;     /* 后气时间(s) 3~15 */
    uint16 arc;         /* 维弧时间(s) 3~15 */
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

/* 在BYS_UART_TX_NEXT_EVT事件处理里调用：清busy并发队列下一包，返回1=已发/还有，0=队列空闲 */
uint8 bys_uart_tx_process(void);

/* 向下位机发送下一条轮询查询：忙或队列非空时不发（轮询包不入队），返回1=未发，0=已发 */
uint8 bys_uart_poll_next(uint8 app_connected);

/* 在OSAL的BYS_UART_RX_EVT事件里调用，解析接收缓冲区 */
void bys_uart_process_rx(void);

/* APP控制指令入队：将12字节包加入发送队列（高优先级，立即发送），返回0成功 */
uint8 bys_uart_send_app_cmd(uint8 *buf, uint8 len);

/* ─── 测试模式接口（无需下位机，模拟数据）────────── */
#define BYS_TEST_DEVICE_TYPE      DEMO_TEST_DEVICE_TYPE
#define BYS_TEST_MODE_MIN         DEMO_TEST_MODE_MIN
#define BYS_TEST_MODE_MAX         DEMO_TEST_MODE_MAX
#define BYS_TEST_T2T4_MIN         DEMO_TEST_T2T4_MIN
#define BYS_TEST_T2T4_MAX         DEMO_TEST_T2T4_MAX
#define BYS_TEST_POSTGAS_MIN      DEMO_TEST_POSTGAS_MIN
#define BYS_TEST_POSTGAS_MAX      DEMO_TEST_POSTGAS_MAX
#define BYS_TEST_ARC_MIN          DEMO_TEST_ARC_MIN
#define BYS_TEST_ARC_MAX          DEMO_TEST_ARC_MAX
#define BYS_TEST_UNIT_MIN         DEMO_TEST_UNIT_MIN
#define BYS_TEST_UNIT_MAX         DEMO_TEST_UNIT_MAX
#define BYS_TEST_ALARM_VALUE      DEMO_TEST_ALARM_VALUE
#define BYS_TEST_ALARM_MIN        DEMO_TEST_ALARM_MIN
#define BYS_TEST_ALARM_MAX        DEMO_TEST_ALARM_MAX
#define BYS_TEST_VOLTAGE_MIN      DEMO_TEST_VOLTAGE_MIN
#define BYS_TEST_VOLTAGE_MAX      DEMO_TEST_VOLTAGE_MAX
#define BYS_TEST_CURRENT_MIN      DEMO_TEST_CURRENT_MIN

#ifdef BYS_TEST_MODE
void bys_test_init(bys_uart_rx_cb_t rx_cb);
void bys_test_poll_next(void);                 /* 100ms：优先出队App指令，空则循帧 */
void bys_test_tick(void);                      /* 10s：三角波步进 */
uint8 bys_test_enqueue_cmd(const uint8 *buf);  /* App下发入队，返回0成功 */
#endif

#endif /* BYS_UART_H */
