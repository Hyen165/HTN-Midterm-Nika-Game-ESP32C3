idf_component_register(
    SRCS "main.c" "ssd1306.c" "input.c" "game.c" "render.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c esp_adc
)