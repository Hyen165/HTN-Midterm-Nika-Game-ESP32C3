#pragma once

/* ---------- Chân & I2C ---------- */
#define PIN_SDA         6
#define PIN_SCL         7
#define I2C_FREQ_HZ     400000
#define OLED_ADDR       0x3C

#define OLED_W          128
#define OLED_H          64

/* ---------- Joystick (ADC1: GPIO3 = CH3, GPIO4 = CH4 trên ESP32-C3) ---------- */
#define JOY1_CH         3
#define JOY2_CH         4
#define JOY1_INVERT     0       /* đổi thành 1 nếu đẩy sang phải mà paddle chạy sang trái */
#define JOY2_INVERT     0
#define JOY1_CENTER     3523
#define JOY2_CENTER     3512
#define JOY_DEADZONE    150

/* ---------- Game ---------- */
#define PADDLE_W        24
#define PADDLE_H        3
#define PADDLE_MAX_SPEED 4      /* px mỗi tick khi đẩy hết cỡ */
#define BALL_SIZE       3
#define BALL_SPEED_Y    1
#define BALL_MAX_VX     1
#define GAME_TICK_MS    16
#define SERVE_TICKS     (1000 / GAME_TICK_MS)
#define OVER_TICKS      (3000 / GAME_TICK_MS)
#define WIN_SCORE       5
