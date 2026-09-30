#pragma once
#include <stdint.h>

void oled_init(void);
void oled_clear(void);
void oled_fill_rect(int x, int y, int w, int h);   /* có clip theo màn hình */
void oled_draw_digit(int x, int y, int digit, int scale);
void oled_flush(void);
