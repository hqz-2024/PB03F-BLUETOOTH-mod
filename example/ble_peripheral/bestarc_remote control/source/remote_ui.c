#include "remote_ui.h"
#include "mini_oled.h"

static uint8_t s_ready = 0;
static uint8_t s_drawn = 0;
static uint8_t s_last_ble_config_mode = 0;
static uint8_t s_last_ble_connected = 0;
static uint8_t s_last_mode_grid = 0;
static uint8_t s_last_ir_on = 0;
static uint8_t s_last_motor_on = 0;
static uint16 s_last_trail_ms = 0;

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
    s_drawn = 0;
    s_ready = oled_init();
}

void remote_ui_update(uint8_t ble_config_mode, uint8_t ble_connected,
                      uint8_t mode_grid, uint8_t ir_on,
                      uint8_t motor_on, uint16 trail_ms)
{
    if (!remote_ui_ready()) {
        return;
    }

    if (s_drawn &&
        s_last_ble_config_mode == ble_config_mode &&
        s_last_ble_connected == ble_connected &&
        s_last_mode_grid == mode_grid &&
        s_last_ir_on == ir_on &&
        s_last_motor_on == motor_on &&
        s_last_trail_ms == trail_ms) {
        return;
    }

    s_drawn = 1;
    s_last_ble_config_mode = ble_config_mode;
    s_last_ble_connected = ble_connected;
    s_last_mode_grid = mode_grid;
    s_last_ir_on = ir_on;
    s_last_motor_on = motor_on;
    s_last_trail_ms = trail_ms;

    oled_clear();
    oled_draw_str(0, 0, "BYS REMOTE");

    oled_draw_str(0, 12, "BLE:");
    if (ble_config_mode) {
        oled_draw_str(30, 12, ble_connected ? "CFG APP" : "CONFIG");
    } else {
        oled_draw_str(30, 12, ble_connected ? "LINK OK" : "SEARCH");
    }

    oled_draw_str(0, 24, "MODE:");
    oled_draw_str(36, 24, mode_grid ? "GRID" : "WELD");

    oled_draw_str(0, 36, "IR:");
    oled_draw_str(24, 36, ir_on ? "ON" : "OFF");
    oled_draw_str(54, 36, "MOTOR:");
    oled_draw_str(96, 36, motor_on ? "ON" : "OFF");

    oled_draw_str(0, 48, "TRAIL:");
    _draw_u16(42, 48, trail_ms);
    oled_draw_str(78, 48, "ms");

    oled_flush();
}

uint8_t remote_ui_ready(void)
{
    return s_ready && oled_ready();
}
