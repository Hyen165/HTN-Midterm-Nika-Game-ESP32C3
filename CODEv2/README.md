# Pong ESP32-C3 (1 board, 2 joystick, 1 OLED SSD1306) - bản 1 file

Toàn bộ mã nguồn nằm trong main/main.c. ESP-IDF >= 5.2.

    idf.py set-target esp32c3
    idf.py build flash monitor

Dây: OLED SDA=GPIO6, SCL=GPIO7; Joystick 1 VRx=GPIO3; Joystick 2 VRx=GPIO4; VCC=3V3, GND chung.
