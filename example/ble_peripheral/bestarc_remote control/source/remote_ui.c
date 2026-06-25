#include "remote_ui.h"
#include "mini_oled.h"

static uint8_t s_ready = 0;

void remote_ui_init(void)
{
    oled_init();
    s_ready = 1;
}

void remote_ui_process(void)
{
    oled_scroll_task();
}

uint8_t remote_ui_ready(void)
{
    return s_ready && oled_ready();
}
