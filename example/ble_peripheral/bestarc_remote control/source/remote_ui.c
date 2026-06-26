#include "remote_ui.h"
#include "mini_oled.h"

static uint8_t s_ready = 0;
static uint8_t s_drawn = 0;
static uint8_t s_last_ble_config_mode = 0;
static uint8_t s_last_ble_connected = 0;
static uint8_t s_last_current_editing = 0;
static uint8_t s_last_current_pending = 0;
static uint8_t s_last_blink_on = 0;
static uint8_t s_last_ir_on = 0;
static uint8_t s_last_motor_on = 0;
static uint16 s_last_edit_current = 0;
static uint16 s_last_trail_ms = 0;
static bys_device_state_t s_last_state;

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

static const char *_mode_text(uint16 mode)
{
    switch (mode) {
    case BYS_MODE_PLATE: return "PLATE";
    case BYS_MODE_GRID:  return "GRID";
    case BYS_MODE_RUST:  return "RUST";
    case BYS_MODE_GOUGE: return "GOUGE";
    default:             return "UNK";
    }
}

static const char *_unit_text(uint16 unit)
{
    switch (unit) {
    case 0: return "PSI";
    case 1: return "MPA";
    case 2: return "BAR";
    default: return "---";
    }
}

static uint8_t _state_same(const bys_device_state_t *state)
{
    return s_last_state.device_type == state->device_type &&
           s_last_state.mode == state->mode &&
           s_last_state.t2t4 == state->t2t4 &&
           s_last_state.current == state->current &&
           s_last_state.postgas == state->postgas &&
           s_last_state.arc == state->arc &&
           s_last_state.unit == state->unit &&
           s_last_state.alarm == state->alarm &&
           s_last_state.voltage == state->voltage &&
           s_last_state.valid == state->valid;
}

void remote_ui_init(void)
{
    s_drawn = 0;
    s_ready = oled_init();
}

void remote_ui_update(uint8_t ble_config_mode, uint8_t ble_connected,
                      const bys_device_state_t *state,
                      uint8_t current_editing, uint8_t current_pending,
                      uint8_t blink_on, uint16 edit_current,
                      uint8_t ir_on, uint8_t motor_on, uint16 trail_ms)
{
    uint16 shown_current;

    if (!remote_ui_ready() || !state) {
        return;
    }

    if (s_drawn &&
        s_last_ble_config_mode == ble_config_mode &&
        s_last_ble_connected == ble_connected &&
        s_last_current_editing == current_editing &&
        s_last_current_pending == current_pending &&
        s_last_blink_on == blink_on &&
        s_last_edit_current == edit_current &&
        s_last_ir_on == ir_on &&
        s_last_motor_on == motor_on &&
        s_last_trail_ms == trail_ms &&
        _state_same(state)) {
        return;
    }

    s_drawn = 1;
    s_last_ble_config_mode = ble_config_mode;
    s_last_ble_connected = ble_connected;
    s_last_current_editing = current_editing;
    s_last_current_pending = current_pending;
    s_last_blink_on = blink_on;
    s_last_edit_current = edit_current;
    s_last_ir_on = ir_on;
    s_last_motor_on = motor_on;
    s_last_trail_ms = trail_ms;
    s_last_state = *state;

    oled_clear();

    if (ble_config_mode) {
        oled_draw_str(0, 0, "BYS REMOTE");
        oled_draw_str(0, 16, ble_connected ? "CFG APP" : "CONFIG");
        oled_draw_str(0, 32, "WAIT APP MAC");
        oled_flush();
        return;
    }

    shown_current = current_editing ? edit_current : state->current;

    oled_draw_str(0, 0, "CUR:");
    if (!current_editing || blink_on) {
        _draw_u16(30, 0, shown_current);
        oled_draw_str(54, 0, "A");
    }
    if (current_pending) {
        oled_draw_str(78, 0, "SEND");
    } else if (current_editing) {
        oled_draw_str(78, 0, "EDIT");
    } else {
        oled_draw_str(78, 0, ble_connected ? "LINK" : "SEARCH");
    }

    oled_draw_str(0, 12, "MODE:");
    oled_draw_str(36, 12, _mode_text(state->mode));
    oled_draw_str(78, 12, state->t2t4 == BYS_T2T4_4T ? "4T" : "2T");

    oled_draw_str(0, 24, "POST:");
    _draw_u16(36, 24, state->postgas);
    oled_draw_str(60, 24, "ARC:");
    _draw_u16(90, 24, state->arc);

    oled_draw_str(0, 36, "V:");
    oled_draw_str(18, 36, state->voltage == BYS_VOLTAGE_120V ? "120" : "240");
    oled_draw_str(42, 36, "U:");
    oled_draw_str(60, 36, _unit_text(state->unit));
    oled_draw_str(90, 36, "AL:");
    _draw_u16(114, 36, state->alarm);

    oled_draw_str(0, 48, "IR:");
    oled_draw_str(18, 48, ir_on ? "ON" : "OFF");
    oled_draw_str(42, 48, "M:");
    oled_draw_str(60, 48, motor_on ? "ON" : "OFF");
    oled_draw_str(84, 48, "T:");
    _draw_u16(102, 48, trail_ms);

    oled_flush();
}

uint8_t remote_ui_ready(void)
{
    return s_ready && oled_ready();
}