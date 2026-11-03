#ifndef BYS_UART_H
#define BYS_UART_H

#include "bcomdef.h"
#include "demo_test_config.h"

/* UART1: P24 TX, P23 RX, MIG145 Pro protocol. */
#define BYS_UART_PORT       UART1
#define BYS_UART_TX_PIN     P24
#define BYS_UART_RX_PIN     P23
#define BYS_UART_BAUD       DEMO_TEST_UART_BAUD

/* Fixed 12-byte protocol frame. */
#define BYS_PKT_LEN         12u
#define BYS_HEADER_0        0xAAu
#define BYS_HEADER_1        0x55u
#define BYS_TAIL_0          0xBBu
#define BYS_TAIL_1          0x55u

#define BYS_DEV_APP_ON      0x8000u
#define BYS_DEV_APP_OFF     0x0000u
#define BYS_DEV_TYPE_MIG145 DEMO_TEST_DEVICE_TYPE

/* Query commands. */
#define BYS_CMD_QUERY_MODE              0x0002u
#define BYS_CMD_QUERY_WIRE_DIAMETER     0x0003u
#define BYS_CMD_QUERY_SMART_MODE        0x0004u
#define BYS_CMD_QUERY_MIG_CURRENT       0x0005u
#define BYS_CMD_QUERY_MIG_VOLTAGE_ADJ   0x0006u
#define BYS_CMD_QUERY_T2T4              0x0007u
#define BYS_CMD_QUERY_TIG_CURRENT       0x0008u
#define BYS_CMD_QUERY_MMA_CURRENT       0x0009u
#define BYS_CMD_QUERY_VRD               0x000Au
#define BYS_CMD_QUERY_MIG_VOLTAGE       0x000Bu
#define BYS_CMD_QUERY_ALARM             0x000Cu
#define BYS_CMD_QUERY_INPUT_VOLTAGE     0x000Du
#define BYS_QUERY_COUNT                 12u

/* Query responses. */
#define BYS_RSP_ERROR                   0x8100u
#define BYS_RSP_MODE                    0x0082u
#define BYS_RSP_WIRE_DIAMETER           0x0083u
#define BYS_RSP_SMART_MODE              0x0084u
#define BYS_RSP_MIG_CURRENT             0x0085u
#define BYS_RSP_MIG_VOLTAGE_ADJ         0x0086u
#define BYS_RSP_T2T4                    0x0087u
#define BYS_RSP_TIG_CURRENT             0x0088u
#define BYS_RSP_MMA_CURRENT             0x0089u
#define BYS_RSP_VRD                     0x008Au
#define BYS_RSP_MIG_VOLTAGE             0x008Bu
#define BYS_RSP_ALARM                   0x008Cu
#define BYS_RSP_INPUT_VOLTAGE           0x008Du

/* App setting commands and confirmations. */
#define BYS_CMD_SET_MODE                0x0200u
#define BYS_CMD_SET_WIRE_DIAMETER       0x0300u
#define BYS_CMD_SET_SMART_MODE          0x0400u
#define BYS_CMD_SET_MIG_CURRENT         0x0500u
#define BYS_CMD_SET_MIG_VOLTAGE_ADJ     0x0600u
#define BYS_CMD_SET_T2T4                0x0700u
#define BYS_CMD_SET_TIG_CURRENT         0x0800u
#define BYS_CMD_SET_MMA_CURRENT         0x0900u
#define BYS_CMD_SET_VRD                 0x0A00u

#define BYS_RSP_SET_MODE                0x8200u
#define BYS_RSP_SET_WIRE_DIAMETER       0x8300u
#define BYS_RSP_SET_SMART_MODE          0x8400u
#define BYS_RSP_SET_MIG_CURRENT         0x8500u
#define BYS_RSP_SET_MIG_VOLTAGE_ADJ     0x8600u
#define BYS_RSP_SET_T2T4                0x8700u
#define BYS_RSP_SET_TIG_CURRENT         0x8800u
#define BYS_RSP_SET_MMA_CURRENT         0x8900u
#define BYS_RSP_SET_VRD                 0x8A00u

#define BYS_CMD_OTA_TRIGGER             0xFE00u
#define BYS_DATA_OTA_TRIGGER            0x00FEu
#define BYS_RSP_ERROR_BUSY              0x0001u

/*
 * Internal/query mode values are 0=MIG, 1=LIFT TIG, 2=MMA.
 * The setting table in the source document uses 0=MMA and 2=MIG; the
 * simulator translates that difference while echoing the App value.
 */
typedef struct {
    uint16 device_type;
    uint16 mode;
    uint16 wire_diameter;
    uint16 smart_mode;
    uint16 mig_current;
    uint16 mig_voltage_adj;
    uint16 t2t4;
    uint16 tig_current;
    uint16 mma_current;
    uint16 vrd;
    uint16 mig_voltage_actual;
    uint16 alarm;
    uint16 input_voltage;
    uint8  valid;
} bys_device_state_t;

extern bys_device_state_t g_bys_state;

typedef void (*bys_uart_rx_cb_t)(uint8 *raw_pkt);

void bys_uart_init(uint8 task_id, uint16 rx_evt, uint16 tx_next_evt,
                   bys_uart_rx_cb_t rx_cb);
void bys_uart_set_rx_callback(bys_uart_rx_cb_t rx_cb);
uint8 bys_uart_tx_process(void);
uint8 bys_uart_poll_next(uint8 app_connected);
void bys_uart_process_rx(void);
uint8 bys_uart_send_app_cmd(uint8 *buf, uint8 len);

/* Used by the hardware-free protocol simulator. */
void bys_uart_emit_response(uint16 cmd, uint16 data);
uint8 bys_packet_valid(const uint8 *pkt);
uint16 bys_setting_mode_to_state(uint16 value);
uint16 bys_uart_query_cmd(uint8 index);

#ifdef BYS_TEST_MODE
void bys_test_init(void);
void bys_test_poll_next(void);
void bys_test_tick(void);
uint8 bys_test_handle_app_cmd(uint8 *buf, uint8 len);
#endif

#endif /* BYS_UART_H */
