/*
 * Pong 2 người chơi trên ESP32-C3 (1 board, 2 joystick, 1 OLED SSD1306)
 * ESP-IDF >= 5.2, FreeRTOS: 3 task (input, game, display) + 2 queue.
 *
 * Bố cục file:
 *   1. Cấu hình          5. Logic game
 *   2. Kiểu dữ liệu      6. Vẽ khung hình
 *   3. Driver OLED       7. Task FreeRTOS
 *   4. Đọc joystick      8. app_main
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "pong";

/* 1. CẤU HÌNH */
/* 1.1 Chân & I2C */
#define PIN_SDA         6
#define PIN_SCL         7
#define I2C_FREQ_HZ     400000
#define OLED_ADDR       0x3C

#define OLED_W          128
#define OLED_H          64

/* 1.2 Joystick */
#define JOY1_CH         3
#define JOY2_CH         4
#define JOY1_INVERT     0       /* đổi thành 1 nếu đẩy sang phải mà paddle chạy sang trái */
#define JOY2_INVERT     0
#define JOY1_CENTER     3510
#define JOY2_CENTER     3526
#define JOY_DEADZONE    70

/* 1.3 Game */
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

/* 2. KIỂU DỮ LIỆU */

/* 2.1. Trục joystick đã chuẩn hóa: -100 (trái hết) .. 0 (giữa) .. +100 (phải hết) */
typedef struct {
    int8_t p1;
    int8_t p2;
} input_msg_t;

typedef enum { PHASE_SERVE, PHASE_PLAY, PHASE_OVER } game_phase_t;

typedef struct {
    int16_t p1_x;               /* paddle trên  (người chơi 1) */
    int16_t p2_x;               /* paddle dưới  (người chơi 2) */
    int16_t ball_x, ball_y;     /* góc trên-trái của bóng */
    int16_t vx, vy;
    uint8_t score1, score2;
    game_phase_t phase;
    uint16_t timer;
} game_state_t;

/* 3. DRIVER OLED SSD1306 */

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

/* Byte đầu = control byte 0x40 (data), nên cả framebuffer gửi được bằng 1 lần transmit */
static uint8_t s_buf[1 + OLED_W * OLED_H / 8];
#define FB (s_buf + 1)

static void oled_send_cmds(const uint8_t *cmds, size_t n)
{
    uint8_t tmp[16];
    tmp[0] = 0x00;                 /* control byte: command */
    memcpy(tmp + 1, cmds, n);
    ESP_ERROR_CHECK(i2c_master_transmit(s_dev, tmp, n + 1, 100));
}

static void oled_clear(void)
{
    memset(FB, 0, OLED_W * OLED_H / 8);
}

static inline void set_pixel(int x, int y)
{
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;
    FB[x + (y >> 3) * OLED_W] |= (1 << (y & 7));
}

static void oled_fill_rect(int x, int y, int w, int h)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            set_pixel(x + i, y + j);
}

/* Font số 3x5, mỗi hàng 3 bit (bit2 = cột trái) */
static const uint8_t DIGITS[10][5] = {
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
    {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7},
};

static void oled_draw_digit(int x, int y, int digit, int scale)
{
    if (digit < 0 || digit > 9) return;
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++)
            if (DIGITS[digit][r] & (4 >> c))
                oled_fill_rect(x + c * scale, y + r * scale, scale, scale);
}

static void oled_flush(void)
{
    static const uint8_t col[]  = {0x21, 0, OLED_W - 1};
    static const uint8_t page[] = {0x22, 0, (OLED_H / 8) - 1};
    oled_send_cmds(col, sizeof(col));
    oled_send_cmds(page, sizeof(page));
    ESP_ERROR_CHECK(i2c_master_transmit(s_dev, s_buf, sizeof(s_buf), 100));
}

static void oled_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev));

    s_buf[0] = 0x40;

    static const uint8_t init_seq[] = {
        0xAE,             /* display off */
        0xD5, 0x80,       /* clock divide */
        0xA8, 0x3F,       /* multiplex 1/64 */
        0xD3, 0x00,       /* display offset */
        0x40,             /* start line 0 */
        0x8D, 0x14,       /* charge pump on */
        0x20, 0x00,       /* horizontal addressing mode */
        0xA1,             /* segment remap */
        0xC8,             /* COM scan dec */
        0xDA, 0x12,       /* COM pins */
        0x81, 0xCF,       /* contrast */
        0xD9, 0xF1,       /* pre-charge */
        0xDB, 0x40,       /* VCOMH */
        0xA4,             /* resume RAM content */
        0xA6,             /* normal (không đảo màu) */
        0xAF,             /* display on */
    };
    /* gửi từng lệnh một (lệnh có tham số thì gửi kèm 1 byte) */
    for (size_t i = 0; i < sizeof(init_seq); ) {
        uint8_t c = init_seq[i];
        size_t n = (c == 0xD5 || c == 0xA8 || c == 0xD3 || c == 0x8D || c == 0x20 ||
                    c == 0xDA || c == 0x81 || c == 0xD9 || c == 0xDB) ? 2 : 1;
        oled_send_cmds(&init_seq[i], n);
        i += n;
    }
    oled_clear();
    oled_flush();
    ESP_LOGI(TAG, "SSD1306 ready (addr 0x%02X, SDA=%d, SCL=%d)", OLED_ADDR, PIN_SDA, PIN_SCL);
}

/* 4. ĐỌC JOYSTICK */

static adc_oneshot_unit_handle_t s_adc;

static void input_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1
    };

    ESP_ERROR_CHECK(
        adc_oneshot_new_unit(&unit_cfg, &s_adc)
    );

    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            s_adc,
            JOY1_CH,
            &ch_cfg
        )
    );

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            s_adc,
            JOY2_CH,
            &ch_cfg
        )
    );
}

static int read_avg(adc_channel_t ch)
{
    int sum = 0;
    int v = 0;

    for (int i = 0; i < 4; i++) {
        ESP_ERROR_CHECK(
            adc_oneshot_read(s_adc, ch, &v)
        );

        sum += v;
    }

    return sum / 4;
}

static int8_t to_axis(
    int raw,
    int center,
    bool invert
)
{
    int d = raw - center;

    if (invert) {
        d = -d;
    }

    /*
     * Deadzone:
     * joystick gần giữa -> đứng yên
     */
    if (d >= -JOY_DEADZONE &&
        d <= JOY_DEADZONE) {
        return 0;
    }

    /*
     * Chuẩn hóa thành -100 ... +100
     */
    int v;

    if (d > 0) {
        v = (d - JOY_DEADZONE) * 100
            / (4095 - center - JOY_DEADZONE);
    } else {
        v = (d + JOY_DEADZONE) * 100
            / (center - JOY_DEADZONE);
    }

    if (v > 100) {v = 100;}

    if (v < -100) {v = -100;}

    return (int8_t)v;
}

static void input_read_axes(input_msg_t *out)
{
    int raw1 = read_avg(JOY1_CH);
    int raw2 = read_avg(JOY2_CH);

    /*
     * In ADC ra Serial để kiểm tra center
     */
    printf(
        "JOY1=%d  JOY2=%d\r\n",
        raw1,
        raw2
    );

    out->p1 = to_axis(
        raw1,
        JOY1_CENTER,
        JOY1_INVERT
    );

    out->p2 = to_axis(
        raw2,
        JOY2_CENTER,
        JOY2_INVERT
    );
}

/* 5. LOGIC GAME */

static void serve(game_state_t *s, int dir_y)
{
    static const int8_t vxs[4] = { -2, -1, 1, 2 };
    s->ball_x = OLED_W / 2 - BALL_SIZE / 2;
    s->ball_y = OLED_H / 2 - BALL_SIZE / 2;
    s->vx = vxs[esp_random() % 4];
    s->vy = dir_y * BALL_SPEED_Y;
    s->phase = PHASE_SERVE;
    s->timer = SERVE_TICKS;
}

static void game_init(game_state_t *s)
{
    s->p1_x = (OLED_W - PADDLE_W) / 2;
    s->p2_x = (OLED_W - PADDLE_W) / 2;
    s->score1 = s->score2 = 0;
    serve(s, +1);
}

static void move_paddle(int16_t *x, int8_t axis)
{
    if (axis == 0) {
        return;   // joystick ở giữa -> đứng yên
    }

    int speed = (abs(axis) * PADDLE_MAX_SPEED) / 100;

    if (speed < 1) {
        speed = 1;
    }

    if (axis > 0) {
        *x += speed;
    } else {
        *x -= speed;
    }

    if (*x < 0)
        *x = 0;

    if (*x > OLED_W - PADDLE_W)
        *x = OLED_W - PADDLE_W;
}

static int overlap_x(int ball_x, int pad_x)
{
    return ball_x + BALL_SIZE > pad_x && ball_x < pad_x + PADDLE_W;
}

/* Góc nảy phụ thuộc vị trí chạm trên paddle */
static int16_t deflect(int ball_x, int pad_x)
{
    int off = (ball_x + BALL_SIZE / 2) - (pad_x + PADDLE_W / 2);
    return off * BALL_MAX_VX / (PADDLE_W / 2);
}

static void point(game_state_t *s, int p1_scored)
{
    if (p1_scored) s->score1++; else s->score2++;

    if (s->score1 >= WIN_SCORE || s->score2 >= WIN_SCORE) {
        s->phase = PHASE_OVER;
        s->timer = OVER_TICKS;
        s->vx = s->vy = 0;
    } else {
        /* giao bóng về phía người vừa để mất điểm */
        serve(s, p1_scored ? +1 : -1);
    }
}

static void step_ball(game_state_t *s)
{
    s->ball_x += s->vx;
    s->ball_y += s->vy;

    if (s->ball_x < 0) { s->ball_x = 0; s->vx = -s->vx; }
    else if (s->ball_x > OLED_W - BALL_SIZE) { s->ball_x = OLED_W - BALL_SIZE; s->vx = -s->vx; }

    /* paddle trên (P1) */
    if (s->vy < 0 && s->ball_y >= 0 && s->ball_y <= PADDLE_H && overlap_x(s->ball_x, s->p1_x)) {
        s->ball_y = PADDLE_H;
        s->vy = -s->vy;
        s->vx = deflect(s->ball_x, s->p1_x);
    }
    /* paddle dưới (P2) */
    else if (s->vy > 0 && s->ball_y + BALL_SIZE >= OLED_H - PADDLE_H &&
             s->ball_y + BALL_SIZE <= OLED_H && overlap_x(s->ball_x, s->p2_x)) {
        s->ball_y = OLED_H - PADDLE_H - BALL_SIZE;
        s->vy = -s->vy;
        s->vx = deflect(s->ball_x, s->p2_x);
    }

    if (s->ball_y + BALL_SIZE <= 0)      point(s, 0);   /* bóng lọt phía P1 -> P2 ghi điểm */
    else if (s->ball_y >= OLED_H)        point(s, 1);   /* bóng lọt phía P2 -> P1 ghi điểm */
}

static void game_update(game_state_t *s, const input_msg_t *in)
{
    move_paddle(&s->p1_x, in->p1);
    move_paddle(&s->p2_x, in->p2);

    switch (s->phase) {
    case PHASE_SERVE:
        if (s->timer > 0) s->timer--;
        if (s->timer == 0) s->phase = PHASE_PLAY;
        break;
    case PHASE_PLAY:
        step_ball(s);
        break;
    case PHASE_OVER:
        if (s->timer > 0) s->timer--;
        if (s->timer == 0) { s->score1 = s->score2 = 0; serve(s, +1); }
        break;
    }
}

/* 6. VẼ KHUNG HÌNH */

static void render_frame(const game_state_t *s)
{
    oled_clear();

    /* lưới giữa sân */
    for (int x = 0; x < OLED_W; x += 8)
        oled_fill_rect(x, OLED_H / 2, 4, 1);

    /* khi kết thúc: paddle của người thắng nhấp nháy */
    int blink_off = (s->phase == PHASE_OVER) && ((s->timer / 16) & 1);
    int p1_won = s->score1 >= WIN_SCORE;
    int p2_won = s->score2 >= WIN_SCORE;

    if (!(blink_off && p1_won))
        oled_fill_rect(s->p1_x, 0, PADDLE_W, PADDLE_H);
    if (!(blink_off && p2_won))
        oled_fill_rect(s->p2_x, OLED_H - PADDLE_H, PADDLE_W, PADDLE_H);

    oled_fill_rect(s->ball_x, s->ball_y, BALL_SIZE, BALL_SIZE);

    /* điểm: P1 nửa trên, P2 nửa dưới, canh trái */
    oled_draw_digit(4, 16, s->score1 % 10, 2);
    oled_draw_digit(4, OLED_H / 2 + 6, s->score2 % 10, 2);
}

/* 7. TASK FREERTOS */

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

/* 8. APP_MAIN */

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
