#include "drivers.h"
#include "board_pins.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "tof";
static bool s_irq;
static bool s_sim = true; /* true=无硬件仿真；接入 ULD 后改为 false */

/* TODO: 接入 ST VL53L5CX Ultra Lite Driver，双地址/双 XSHUT */

esp_err_t tof_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_PORT_NUM, &conf);
    if (err != ESP_OK) return err;
    err = i2c_driver_install(I2C_PORT_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    gpio_set_level(PIN_TOF_A_XSHUT, 0);
    gpio_set_level(PIN_TOF_B_XSHUT, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_TOF_A_XSHUT, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    /* 此处应对 A 改 I2C 地址，再拉起 B */
    gpio_set_level(PIN_TOF_B_XSHUT, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_LOGI(TAG, "tof init (%s)", s_sim ? "SIM" : "HW");
    return ESP_OK;
}

static void fill_empty(uint16_t d[APP_TOF_ZONE_N])
{
    for (int i = 0; i < APP_TOF_ZONE_N; i++) {
        d[i] = 2200; /* 空场约 2.2m */
    }
}

esp_err_t tof_read_both(uint16_t a[APP_TOF_ZONE_N], uint16_t b[APP_TOF_ZONE_N])
{
    fill_empty(a);
    fill_empty(b);

    if (s_sim) {
        /* 仿真：周期性在门线两侧移动一个目标，便于无传感器调试计数 */
        int64_t us = esp_timer_get_time();
        int phase = (int)((us / 200000) % 20);
        int y = (phase < 10) ? phase : (19 - phase); /* 0..9..0 */
        int x = 3;
        int idx = y * 8 + x;
        if (idx >= 0 && idx < APP_TOF_ZONE_N) {
            a[idx] = 1200;
            if (idx + 1 < APP_TOF_ZONE_N) a[idx + 1] = 1200;
        }
        /* B 阵列偏移一点，模拟双机 */
        int yb = y;
        int ib = yb * 8 + x;
        if (ib >= 0 && ib < APP_TOF_ZONE_N) {
            b[ib] = 1250;
        }
        return ESP_OK;
    }

    /* TODO: ULD GetRangingData for sensor A/B */
    return ESP_OK;
}

esp_err_t tof_enter_autonomous_irq(void)
{
    ESP_LOGI(TAG, "autonomous irq mode");
    /* TODO: ULD 低帧率 + threshold interrupt */
    return ESP_OK;
}

esp_err_t tof_enter_active_mode(void)
{
    ESP_LOGI(TAG, "active ranging mode");
    return ESP_OK;
}

bool tof_irq_pending(void)
{
    if (gpio_get_level(PIN_TOF_INT) == 0) {
        s_irq = true;
    }
    return s_irq;
}

void tof_irq_clear(void)
{
    s_irq = false;
}