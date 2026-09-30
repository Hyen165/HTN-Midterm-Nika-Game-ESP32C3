#include "ssd1306.h"
#include "config.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ssd1306";

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

/* Byte đầu = control byte 0x40 (data), nên cả framebuffer gửi được bằng 1 lần transmit */
static uint8_t s_buf[1 + OLED_W * OLED_H / 8];
#define FB (s_buf + 1)

static void send_cmds(const uint8_t *cmds, size_t n)
{
    uint8_t tmp[16];
    tmp[0] = 0x00;                 /* control byte: command */
    memcpy(tmp + 1, cmds, n);
    ESP_ERROR_CHECK(i2c_master_transmit(s_dev, tmp, n + 1, 100));
}

void oled_init(void)
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
    /* gửi từng lệnh một cho đơn giản */
    for (size_t i = 0; i < sizeof(init_seq); ) {
        uint8_t c = init_seq[i];
        size_t n = (c == 0xD5 || c == 0xA8 || c == 0xD3 || c == 0x8D || c == 0x20 ||
                    c == 0xDA || c == 0x81 || c == 0xD9 || c == 0xDB) ? 2 : 1;
        send_cmds(&init_seq[i], n);
        i += n;
    }
    oled_clear();
    oled_flush();
    ESP_LOGI(TAG, "SSD1306 ready (addr 0x%02X, SDA=%d, SCL=%d)", OLED_ADDR, PIN_SDA, PIN_SCL);
}

void oled_clear(void)
{
    memset(FB, 0, OLED_W * OLED_H / 8);
}

static inline void set_pixel(int x, int y)
{
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;
    FB[x + (y >> 3) * OLED_W] |= (1 << (y & 7));
}

void oled_fill_rect(int x, int y, int w, int h)
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

void oled_draw_digit(int x, int y, int digit, int scale)
{
    if (digit < 0 || digit > 9) return;
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++)
            if (DIGITS[digit][r] & (4 >> c))
                oled_fill_rect(x + c * scale, y + r * scale, scale, scale);
}

void oled_flush(void)
{
    static const uint8_t col[] = {0x21, 0, OLED_W - 1};
    static const uint8_t page[] = {0x22, 0, (OLED_H / 8) - 1};
    send_cmds(col, sizeof(col));
    send_cmds(page, sizeof(page));
    ESP_ERROR_CHECK(i2c_master_transmit(s_dev, s_buf, sizeof(s_buf), 100));
}
