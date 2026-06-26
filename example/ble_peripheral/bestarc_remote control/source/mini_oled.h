#ifndef MINI_OLED_H
#define MINI_OLED_H

#include "types.h"

uint8_t oled_init(void);
void oled_clear(void);
void oled_set_pixel(uint8_t x, uint8_t y, uint8_t on);
void oled_draw_char(uint8_t x, uint8_t y, char c);
void oled_draw_str(uint8_t x, uint8_t y, const char* str);
uint8_t oled_flush(void);
uint8_t oled_ready(void);

#endif
