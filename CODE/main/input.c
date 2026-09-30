#include "input.h"
#include "config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include <stdbool.h>
#include <stdio.h>

static adc_oneshot_unit_handle_t s_adc;

void input_init(void)
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

    if (v > 100) {
        v = 100;
    }

    if (v < -100) {
        v = -100;
    }

    return (int8_t)v;
}

void input_read_axes(input_msg_t *out)
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