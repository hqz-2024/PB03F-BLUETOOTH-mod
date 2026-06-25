/* ─── 极简 SSD1306 128x64 I2C OLED 驱动 ───────────────── */
#ifndef MINI_OLED_H
#define MINI_OLED_H

#include "types.h"

void oled_init(void);
void oled_clear(void);
void oled_set_pixel(uint8_t x, uint8_t y, uint8_t on);
void oled_draw_char(uint8_t x, uint8_t y, char c);
void oled_draw_str(uint8_t x, uint8_t y, const char* str);
void oled_flush(void);             /* 把 framebuffer 推到屏幕 */
void oled_scroll_task(void);       /* 每 50ms 调用一次: "happy birthday" 左滚 */

uint8_t oled_ready(void);

#endif
