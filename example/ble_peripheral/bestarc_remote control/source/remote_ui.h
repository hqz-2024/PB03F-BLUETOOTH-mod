#ifndef REMOTE_UI_H
#define REMOTE_UI_H

#include "types.h"
#include "remote_proto.h"

void remote_ui_init(void);
void remote_ui_update(uint8_t ble_config_mode, uint8_t ble_connected,
                      const bys_device_state_t *state,
                      uint8_t current_editing, uint8_t current_pending,
                      uint8_t blink_on, uint16 edit_current,
                      uint8_t ir_on, uint8_t motor_on, uint16 trail_ms);
uint8_t remote_ui_ready(void);

#endif