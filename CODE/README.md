# Pong ESP32-C3 (1 board, 2 joystick, 1 OLED SSD1306)

Yêu cầu: ESP-IDF >= 5.2 (dùng driver i2c_master mới + adc_oneshot).

## Đấu dây
| Thiết bị | Chân | GPIO |
|---|---|---|
| OLED SSD1306 | SDA / SCL | 6 / 7 |
| Joystick 1 | VRx | 3 |
| Joystick 2 | VRx | 4 |
VCC = 3.3V, GND chung.

## Build
    idf.py set-target esp32c3
    idf.py build flash monitor

Đổi chân/tốc độ/kích thước trong main/config.h.
