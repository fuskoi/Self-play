#include "drivers.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "board_io";

esp_err_t board_io_init(void)
{
    gpio_config_t out = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_LTE_PWR_EN) | (1ULL << PIN_LTE_PWRKEY) |
                        (1ULL << PIN_CAM_PWR_EN) | (1ULL << PIN_BUZZER) |
                        (1ULL << PIN_TOF_A_XSHUT) | (1ULL << PIN_TOF_B_XSHUT),
    };
    ESP_ERROR_CHECK(gpio_config(&out));

    gpio_config_t in = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_KEY1) | (1ULL << PIN_KEY2) | (1ULL << PIN_TOF_INT),
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in));

    gpio_set_level(PIN_LTE_PWR_EN, 0);
    gpio_set_level(PIN_CAM_PWR_EN, 0);
    gpio_set_level(PIN_BUZZER, 0);
    gpio_set_level(PIN_TOF_A_XSHUT, 1);
    gpio_set_level(PIN_TOF_B_XSHUT, 1);

    ESP_LOGI(TAG, "board io ok");
    return ESP_OK;
}

bool board_key1_pressed(void)
{
    return gpio_get_level(PIN_KEY1) == 0;
}

bool board_key2_pressed(void)
{
    return gpio_get_level(PIN_KEY2) == 0;
}

void board_buzzer_beep(int pattern)
{
    /* 模式 1=超员间歇鸣，2=低电三短声 */
    if (pattern == 2) {
        for (int i = 0; i < 3; i++) {
            gpio_set_level(PIN_BUZZER, 1);
            vTaskDelay(pdMS_TO_TICKS(80));
            gpio_set_level(PIN_BUZZER, 0);
            vTaskDelay(pdMS_TO_TICKS(80));
        }
        return;
    }
    gpio_set_level(PIN_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(PIN_BUZZER, 0);
}

void board_lte_power(bool on)
{
    gpio_set_level(PIN_LTE_PWR_EN, on ? 1 : 0);
}

void board_cam_power(bool on)
{
    gpio_set_level(PIN_CAM_PWR_EN, on ? 1 : 0);
}