#ifndef REMOTE_APP_H
#define REMOTE_APP_H

#include "bcomdef.h"

/* OSAL event bits */
#define REMOTE_START_EVT            0x0001u
#define REMOTE_ADC_READ_EVT         0x0002u
#define REMOTE_IR_CHECK_EVT         0x0004u
#define REMOTE_TRAIL_STOP_EVT       0x0008u
#define REMOTE_BLE_EVT              0x0010u
#define REMOTE_BTN_POLL_EVT         0x0020u
#define REMOTE_RECONNECT_EVT        0x0040u
#define REMOTE_CONFIG_RESET_EVT     0x0080u
#define REMOTE_UI_EVT               0x0100u
#define REMOTE_LINK_GUARD_EVT       0x0200u

/* Timing */
#define REMOTE_ADC_INTERVAL_MS      100u
#define REMOTE_IR_POLL_MS           20u
#define REMOTE_BTN_POLL_MS          50u
#define REMOTE_UI_INTERVAL_MS       100u
#define REMOTE_UI_DATA_FLUSH_MIN_MS 1500u

typedef enum {
    APP_STATE_IDLE      = 0,
    APP_STATE_RUNNING   = 1,
    APP_STATE_TRAILING  = 2
} app_state_e;

void   Remote_Init(uint8 task_id);
uint16 Remote_ProcessEvent(uint8 task_id, uint16 events);
int    app_main(void);

#endif /* REMOTE_APP_H */
