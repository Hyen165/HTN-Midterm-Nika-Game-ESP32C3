#pragma once
#include <stdint.h>

/* Trục joystick đã chuẩn hóa: -100 (trái hết) .. 0 (giữa/deadzone) .. +100 (phải hết) */
typedef struct {
    int8_t p1;
    int8_t p2;
} input_msg_t;

void input_init(void);
void input_read_axes(input_msg_t *out);
