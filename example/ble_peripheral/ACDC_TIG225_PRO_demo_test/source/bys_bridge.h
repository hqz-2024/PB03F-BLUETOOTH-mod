#ifndef BYS_BRIDGE_H
#define BYS_BRIDGE_H

#include "bcomdef.h"

/* Comment this line out to bridge a real ACDC TIG225 Pro over UART1. */
#define BYS_TEST_MODE

#define BYS_START_DEVICE_EVT    0x0001u
#define BYS_RESET_ADV_EVT       0x0002u
#define BYS_POLL_TIMER_EVT      0x0004u
#define BYS_UART_RX_EVT         0x0008u
#define BYS_UART_TX_NEXT_EVT    0x0010u
#ifdef BYS_TEST_MODE
#define BYS_BUTTON_EVT          0x0020u
#endif
#define BYS_LED_EVT             0x0040u
#define BYS_APP_CMD_EVT         0x0080u

#define BYS_LED_PIN             P0
#define BYS_LED_TOGGLE_MS       250u
#define BYS_POLL_INTERVAL_MS    100u
#define BYS_RESET_ADV_DELAY_MS  100u

#ifdef BYS_TEST_MODE
#define BYS_BUTTON_PIN          P15
#define BYS_BUTTON_DEBOUNCE_MS  50u
#endif

/* Absolute advertData offsets: Flags(3) + Name(5) + AD3 length/type(2). */
#define ADV_AD3_START            8
#define ADV_MAC_OFFSET           10
#define ADV_DEV_TYPE_OFFSET      16
#define ADV_MODE_OFFSET          18
#define ADV_T2T4_OFFSET          20
#define ADV_CURRENT_OFFSET       22
#define ADV_PULSE_OFFSET         24
#define ADV_ALARM_OFFSET         26
#define ADV_INPUT_VOLTAGE_OFFSET 28

void BYS_Bridge_Init(uint8 task_id);
uint16 BYS_Bridge_ProcessEvent(uint8 task_id, uint16 events);

#endif /* BYS_BRIDGE_H */
