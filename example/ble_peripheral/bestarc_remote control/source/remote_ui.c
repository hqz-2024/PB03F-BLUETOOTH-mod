#include "remote_ui.h"
#include "mini_oled.h"

#define UI_ROW_CUR      2u
#define UI_ROW_T2T4     12u
#define UI_ROW_POST     22u
#define UI_ROW_ARC      32u
#define UI_ROW_VIN      42u

static uint8_t s_ready = 0;

static void _draw_u16(uint8_t x, uint8_t y, uint16 value)
{
    char buf[6];
    uint8_t i = 0;
    uint8_t j;

    if (value == 0) {
        oled_draw_char(x, y, '0');
        return;
    }

    while (value && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (j = 0; j < i; j++) {
        oled_draw_char((uint8_t)(x + j * 6), y, buf[i - 1 - j]);
    }
}

void remote_ui_init(void)
{
    s_ready = oled_init();
}

void remote_ui_update(uint8_t ble_config_mode, uint8_t ble_connected,
                      const bys_device_state_t *state,
                      uint8_t current_editing, uint8_t current_pending,
                      uint8_t blink_on, uint16 edit_current,
                      uint8_t ir_on, uint8_t motor_on, uint16 trail_ms)
{
    uint16 shown_current;

    (void)current_pending;
    (void)ir_on;
    (void)motor_on;
    (void)trail_ms;

    if (!remote_ui_ready() || !state) {
        return;
    }

    oled_clear();

    if (ble_config_mode) {
        oled_draw_str(0, 0, "BYS REMOTE");
        oled_draw_str(0, 16, ble_connected ? "CFG APP" : "CONFIG");
        oled_draw_str(0, 32, "WAIT APP MAC");
        oled_request_flush_pages(0, 8);
        return;
    }

    shown_current = current_editing ? edit_current : state->current;

    oled_draw_str(0, UI_ROW_CUR, "CUR:");
    if (!current_editing || blink_on) {
        _draw_u16(36, UI_ROW_CUR, shown_current);
        oled_draw_str(66, UI_ROW_CUR, "A");
    }

    oled_draw_str(0, UI_ROW_T2T4, "T:");
    oled_draw_str(36, UI_ROW_T2T4, state->t2t4 == BYS_T2T4_4T ? "4T" : "2T");

    oled_draw_str(0, UI_ROW_POST, "POST:");
    _draw_u16(36, UI_ROW_POST, state->postgas);
    oled_draw_str(66, UI_ROW_POST, "S");

    oled_draw_str(0, UI_ROW_ARC, "ARC:");
    _draw_u16(36, UI_ROW_ARC, state->arc);
    oled_draw_str(66, UI_ROW_ARC, "S");

    oled_draw_str(0, UI_ROW_VIN, "VIN:");
    oled_draw_str(36, UI_ROW_VIN, state->voltage == BYS_VOLTAGE_120V ? "120V" : "240V");

    oled_request_flush_pages(0, 8);
}

uint8_t remote_ui_ready(void)
{
    return s_ready && oled_ready();
}

uint8_t remote_ui_flush_pending(void)
{
    return oled_flush_pending();
}
