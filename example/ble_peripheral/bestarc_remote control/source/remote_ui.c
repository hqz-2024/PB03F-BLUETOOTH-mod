#include "remote_ui.h"
#include "mini_oled.h"

#define UI_ROW_CUR      2u
#define UI_ROW_T2T4     12u
#define UI_ROW_POST     22u
#define UI_ROW_ARC      32u
#define UI_ROW_VIN      42u
#define UI_ARROW_X      78u

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
                      uint8_t arrow_row, uint8_t edit_mode,
                      uint8_t blink_on,
                      uint16_t edit_current,
                      uint16_t edit_t2t4,
                      uint16_t edit_postgas,
                      uint16_t edit_arc,
                      uint16_t edit_voltage,
                      const char *dev_name)
{
    uint8_t editing_cur, editing_t2t4, editing_post, editing_arc;
    uint16_t show_cur, show_t2t4, show_post, show_arc, show_vin;
    uint8_t show_cur_blink, show_t2t4_blink, show_post_blink, show_arc_blink;

    if (!remote_ui_ready() || !state) {
        return;
    }

    editing_cur   = edit_mode && (arrow_row == UI_ARROW_CUR);
    editing_t2t4  = edit_mode && (arrow_row == UI_ARROW_T2T4);
    editing_post  = edit_mode && (arrow_row == UI_ARROW_POST);
    editing_arc   = edit_mode && (arrow_row == UI_ARROW_ARC);

    show_cur   = editing_cur   ? edit_current  : state->current;
    show_t2t4  = editing_t2t4  ? edit_t2t4     : state->t2t4;
    show_post  = editing_post  ? edit_postgas  : state->postgas;
    show_arc   = editing_arc   ? edit_arc      : state->arc;
    show_vin   = state->voltage;

    show_cur_blink  = !editing_cur  || blink_on;
    show_t2t4_blink = !editing_t2t4 || blink_on;
    show_post_blink = !editing_post || blink_on;
    show_arc_blink  = !editing_arc  || blink_on;

    oled_clear();

    if (ble_config_mode) {
        oled_draw_str(0, 0, "BYS REMOTE");
        oled_draw_str(0, 16, ble_connected ? "CFG APP" : "CONFIG");
        oled_draw_str(0, 32, "WAIT APP MAC");
        oled_request_flush_pages(0, 8);
        return;
    }

    /* Row 0: CUR */
    oled_draw_str(0, UI_ROW_CUR, "CUR :");
    if (show_cur_blink) {
        _draw_u16(36, UI_ROW_CUR, show_cur);
        oled_draw_str(66, UI_ROW_CUR, "A");
    }
    if (arrow_row == UI_ARROW_CUR) {
        oled_draw_char(UI_ARROW_X, UI_ROW_CUR, '<');
    }

    /* Row 1: T */
    oled_draw_str(0, UI_ROW_T2T4, "MODE:");
    if (show_t2t4_blink) {
        oled_draw_str(36, UI_ROW_T2T4, show_t2t4 == BYS_T2T4_4T ? "4T" : "2T");
    }
    if (arrow_row == UI_ARROW_T2T4) {
        oled_draw_char(UI_ARROW_X, UI_ROW_T2T4, '<');
    }

    /* Row 2: POST */
    oled_draw_str(0, UI_ROW_POST, "POST:");
    if (show_post_blink) {
        _draw_u16(36, UI_ROW_POST, show_post);
        oled_draw_str(66, UI_ROW_POST, "S");
    }
    if (arrow_row == UI_ARROW_POST) {
        oled_draw_char(UI_ARROW_X, UI_ROW_POST, '<');
    }

    /* Row 3: ARC */
    oled_draw_str(0, UI_ROW_ARC, "ARC :");
    if (show_arc_blink) {
        _draw_u16(36, UI_ROW_ARC, show_arc);
        oled_draw_str(66, UI_ROW_ARC, "S");
    }
    if (arrow_row == UI_ARROW_ARC) {
        oled_draw_char(UI_ARROW_X, UI_ROW_ARC, '<');
    }

    /* Row 4: VIN (display only) */
    oled_draw_str(0, UI_ROW_VIN, "VIN :");
    oled_draw_str(36, UI_ROW_VIN, show_vin == BYS_VOLTAGE_120V ? "120V" : "240V");

    /* 设备型号 (右下角) */
    if (dev_name && dev_name[0]) {
        uint8_t name_len = 0;
        while (dev_name[name_len]) name_len++;
        oled_draw_str((uint8_t)(128u - name_len * 6u), 54, dev_name);
    }

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
