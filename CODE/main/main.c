#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "config.h"
#include "ssd1306.h"
#include "input.h"
#include "game.h"
#include "render.h"

static const char *TAG = "pong";

/* Cả hai queue dài 1 + xQueueOverwrite => luôn giữ giá trị mới nhất, không bao giờ đầy */
static QueueHandle_t s_input_q;
static QueueHandle_t s_state_q;

static void input_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        input_msg_t m;
        input_read_axes(&m);
        xQueueOverwrite(s_input_q, &m);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
    }
}

static void game_task(void *arg)
{
    game_state_t s;
    game_init(&s);
    input_msg_t in = {0};
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        xQueuePeek(s_input_q, &in, 0);          /* nếu chưa có thì dùng giá trị cũ */
        game_update(&s, &in);
        xQueueOverwrite(s_state_q, &s);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(GAME_TICK_MS));
    }
}

static void display_task(void *arg)
{
    game_state_t s;
    for (;;) {
        if (xQueueReceive(s_state_q, &s, portMAX_DELAY) == pdTRUE) {
            render_frame(&s);
            oled_flush();                       /* ~20-25 ms qua I2C 400 kHz */
        }
    }
}

void app_main(void)
{
    s_input_q = xQueueCreate(1, sizeof(input_msg_t));
    s_state_q = xQueueCreate(1, sizeof(game_state_t));

    oled_init();
    input_init();

    /* ESP32-C3 chỉ có 1 core nên dùng xTaskCreate là đủ */
    xTaskCreate(input_task,   "input",   3072, NULL, 4, NULL);
    xTaskCreate(game_task,    "game",    4096, NULL, 5, NULL);
    xTaskCreate(display_task, "display", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "started");
}
