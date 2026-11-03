#ifndef BYS_UART_H
#define BYS_UART_H

#include "bcomdef.h"
#include "demo_test_config.h"

/* UART1: P24 TX, P23 RX, ACDC TIG225 Pro protocol. */
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
#define BYS_DEV_TYPE_TIG225 DEMO_TEST_DEVICE_TYPE

/* Query commands. */
#define BYS_CMD_QUERY_MODE              0x0002u
#define BYS_CMD_QUERY_HOT_START         0x0003u
#define BYS_CMD_QUERY_THRUST            0x0004u
#define BYS_CMD_QUERY_VRD               0x0005u
#define BYS_CMD_QUERY_WELD_CURRENT      0x0006u
#define BYS_CMD_QUERY_T2T4_SPOT         0x0007u
#define BYS_CMD_QUERY_PULSE             0x0008u
#define BYS_CMD_QUERY_SPOT_TIME         0x0009u
#define BYS_CMD_QUERY_PRE_GAS           0x000Au
#define BYS_CMD_QUERY_START_CURRENT     0x000Bu
#define BYS_CMD_QUERY_UP_SLOPE          0x000Cu
#define BYS_CMD_QUERY_PULSE_DUTY        0x000Du
#define BYS_CMD_QUERY_PULSE_FREQ        0x000Eu
#define BYS_CMD_QUERY_PULSE_VALLEY      0x000Fu
#define BYS_CMD_QUERY_AC_FREQ           0x0010u
#define BYS_CMD_QUERY_AC_DUTY           0x0011u
#define BYS_CMD_QUERY_DOWN_SLOPE        0x0012u
#define BYS_CMD_QUERY_END_CURRENT       0x0013u
#define BYS_CMD_QUERY_POST_GAS          0x0014u
#define BYS_CMD_QUERY_ALARM             0x0015u
#define BYS_CMD_QUERY_INPUT_VOLTAGE     0x0016u
#define BYS_QUERY_COUNT                 21u

/* Query responses. */
#define BYS_RSP_ERROR                   0x8100u
#define BYS_RSP_MODE                    0x0082u
#define BYS_RSP_HOT_START               0x0083u
#define BYS_RSP_THRUST                  0x0084u
#define BYS_RSP_VRD                     0x0085u
#define BYS_RSP_WELD_CURRENT            0x0086u
#define BYS_RSP_T2T4_SPOT               0x0087u
#define BYS_RSP_PULSE                   0x0088u
#define BYS_RSP_SPOT_TIME               0x0089u
#define BYS_RSP_PRE_GAS                 0x008Au
#define BYS_RSP_START_CURRENT           0x008Bu
#define BYS_RSP_UP_SLOPE                0x008Cu
#define BYS_RSP_PULSE_DUTY              0x008Du
#define BYS_RSP_PULSE_FREQ              0x008Eu
#define BYS_RSP_PULSE_VALLEY            0x008Fu
#define BYS_RSP_AC_FREQ                 0x0090u
#define BYS_RSP_AC_DUTY                 0x0091u
#define BYS_RSP_DOWN_SLOPE              0x0092u
#define BYS_RSP_END_CURRENT             0x0093u
#define BYS_RSP_POST_GAS                0x0094u
#define BYS_RSP_ALARM                   0x0095u
#define BYS_RSP_INPUT_VOLTAGE           0x0096u

/* App setting commands and confirmations. */
#define BYS_CMD_SET_MODE                0x0200u
#define BYS_CMD_SET_HOT_START           0x0300u
#define BYS_CMD_SET_THRUST              0x0400u
#define BYS_CMD_SET_VRD                 0x0500u
#define BYS_CMD_SET_WELD_CURRENT        0x0600u
#define BYS_CMD_SET_T2T4_SPOT           0x0700u
#define BYS_CMD_SET_PULSE               0x0800u
#define BYS_CMD_SET_SPOT_TIME           0x0900u
#define BYS_CMD_SET_PRE_GAS             0x0A00u
#define BYS_CMD_SET_START_CURRENT       0x0B00u
#define BYS_CMD_SET_UP_SLOPE            0x0C00u
#define BYS_CMD_SET_PULSE_DUTY          0x0D00u
#define BYS_CMD_SET_PULSE_FREQ          0x0E00u
#define BYS_CMD_SET_PULSE_VALLEY        0x0F00u
#define BYS_CMD_SET_AC_FREQ             0x1000u
#define BYS_CMD_SET_AC_DUTY             0x1100u
#define BYS_CMD_SET_DOWN_SLOPE          0x1200u
#define BYS_CMD_SET_END_CURRENT         0x1300u
#define BYS_CMD_SET_POST_GAS            0x1400u

#define BYS_RSP_SET_MODE                0x8200u
#define BYS_RSP_SET_HOT_START           0x8300u
#define BYS_RSP_SET_THRUST              0x8400u
#define BYS_RSP_SET_VRD                 0x8500u
#define BYS_RSP_SET_WELD_CURRENT        0x8600u
#define BYS_RSP_SET_T2T4_SPOT           0x8700u
#define BYS_RSP_SET_PULSE               0x8800u
#define BYS_RSP_SET_SPOT_TIME           0x8900u
#define BYS_RSP_SET_PRE_GAS             0x8A00u
#define BYS_RSP_SET_START_CURRENT       0x8B00u
#define BYS_RSP_SET_UP_SLOPE            0x8C00u
#define BYS_RSP_SET_PULSE_DUTY          0x8D00u
#define BYS_RSP_SET_PULSE_FREQ          0x8E00u
#define BYS_RSP_SET_PULSE_VALLEY        0x8F00u
#define BYS_RSP_SET_AC_FREQ             0x9000u
#define BYS_RSP_SET_AC_DUTY             0x9100u
#define BYS_RSP_SET_DOWN_SLOPE          0x9200u
#define BYS_RSP_SET_END_CURRENT         0x9300u
#define BYS_RSP_SET_POST_GAS            0x9400u

#define BYS_RSP_ERROR_BUSY              0x0001u

typedef struct {
    uint16 device_type;
    uint16 mode;
    uint16 hot_start;
    uint16 thrust;
    uint16 vrd;
    uint16 weld_current;
    uint16 t2t4_spot;
    uint16 pulse;
    uint16 spot_time;
    uint16 pre_gas;
    uint16 start_current;
    uint16 up_slope;
    uint16 pulse_duty;
    uint16 pulse_freq;
    uint16 pulse_valley;
    uint16 ac_freq;
    uint16 ac_duty;
    uint16 down_slope;
    uint16 end_current;
    uint16 post_gas;
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

void bys_uart_emit_response(uint16 cmd, uint16 data);
uint8 bys_packet_valid(const uint8 *pkt);
uint16 bys_uart_query_cmd(uint8 index);

#ifdef BYS_TEST_MODE
void bys_test_init(void);
void bys_test_poll_next(void);
void bys_test_tick(void);
uint8 bys_test_handle_app_cmd(uint8 *buf, uint8 len);
#endif

#endif /* BYS_UART_H */
