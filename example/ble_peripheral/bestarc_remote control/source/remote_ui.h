#ifndef REMOTE_UI_H
#define REMOTE_UI_H

#include "types.h"
#include "remote_proto.h"

/* Arrow row indices (VIN is display-only, not selectable) */
#define UI_ARROW_CUR    0u
#define UI_ARROW_T2T4   1u
#define UI_ARROW_POST   2u
#define UI_ARROW_ARC    3u
#define UI_ARROW_COUNT  4u

void remote_ui_init(void);
void remote_ui_update(uint8_t ble_config_mode, uint8_t ble_connected,
                      const bys_device_state_t *state,
                      uint8_t arrow_row, uint8_t edit_mode,
                      uint8_t blink_on,
                      uint16_t edit_current,
                      uint16_t edit_t2t4,
                      uint16_t edit_postgas,
                      uint16_t edit_arc,
                      uint16_t edit_voltage,
                      const char *dev_name);
uint8_t remote_ui_ready(void);
uint8_t remote_ui_flush_pending(void);

#endif
