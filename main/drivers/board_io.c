#include "drivers.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "board_io";
static adc_oneshot_unit_handle_t s_adc;
static bool s_adc_ok;

esp_err_t board_adc_read(adc_channel_t ch, int *raw)
{
    if (!s_adc_ok || !raw) {
        return ESP_ERR_INVALID_STATE;
    }
    return adc_oneshot_read(s_adc, ch, raw);
}

static void out_add(uint64_t *m, int pin)
{
    if (pin >= 0) {
        *m |= (1ULL << pin);
    }
}

static uint64_t out_mask(void)
{
    uint64_t m = 0;
    out_add(&m, PIN_LTE_PWR_EN);
    out_add(&m, PIN_CAM_PWR_EN);
    out_add(&m, PIN_BUZZER);
    out_add(&m, PIN_TOF_A_XSHUT);
    out_add(&m, PIN_TOF_B_XSHUT);
    out_add(&m, PIN_LTE_RST);
    out_add(&m, PIN_CHAG_EN);
    return m;
}

static void gpio_set_level_opt(int pin, int level)
{
    if (pin >= 0) {
        gpio_set_level((gpio_num_t)pin, level);
    }
}

esp_err_t board_io_init(void)
{
    uint64_t mask = out_mask();
    if (mask) {
        gpio_config_t out = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = mask,
        };
        ESP_ERROR_CHECK(gpio_config(&out));
    }

    if (PIN_TOF_INT >= 0) {
        gpio_config_t in = {
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = (1ULL << PIN_TOF_INT),
            .pull_up_en = GPIO_PULLUP_ENABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&in));
    }

    gpio_set_level_opt(PIN_LTE_PWR_EN, 0);
    gpio_set_level_opt(PIN_LTE_RST, 1);
    gpio_set_level_opt(PIN_CAM_PWR_EN, 0);
    gpio_set_level_opt(PIN_BUZZER, 0);
    gpio_set_level_opt(PIN_TOF_A_XSHUT, 1);
    gpio_set_level_opt(PIN_TOF_B_XSHUT, 1);
    gpio_set_level_opt(PIN_CHAG_EN, 1);

    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = ADC_UNIT_1 };
    if (adc_oneshot_new_unit(&ucfg, &s_adc) == ESP_OK) {
        adc_oneshot_chan_cfg_t ch = {
            .bitwidth = ADC_BITWIDTH_DEFAULT,
            .atten = ADC_ATTEN_DB_12,
        };
        bool ok_bat = false;
        bool ok_key = false;
#if PIN_BAT_ADC >= 0
        ok_bat = adc_oneshot_config_channel(s_adc, BAT_ADC_CHANNEL, &ch) == ESP_OK;
#endif
#if PIN_KEY_ADC >= 0
        ok_key = adc_oneshot_config_channel(s_adc, KEY_ADC_CHANNEL, &ch) == ESP_OK;
#endif
        s_adc_ok = ok_bat || ok_key;
    }

    ESP_LOGI(TAG, "board io ok (adc=%d)", (int)s_adc_ok);
    return ESP_OK;
}

static int key_adc_raw(void)
{
    int raw = 4095;
    if (board_adc_read(KEY_ADC_CHANNEL, &raw) != ESP_OK) {
        return 4095;
    }
    return raw;
}

bool board_key1_pressed(void)
{
    int r = key_adc_raw();
    return (r > 200 && r < 1100);
}

bool board_key2_pressed(void)
{
    int r = key_adc_raw();
    return (r > 1400 && r < 2800);
}

void board_buzzer_beep(int pattern)
{
    if (PIN_BUZZER < 0) {
        return;
    }
    if (pattern == 2) {
        for (int i = 0; i < 3; i++) {
            gpio_set_level_opt(PIN_BUZZER, 1);
            vTaskDelay(pdMS_TO_TICKS(80));
            gpio_set_level_opt(PIN_BUZZER, 0);
            vTaskDelay(pdMS_TO_TICKS(80));
        }
        return;
    }
    gpio_set_level_opt(PIN_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level_opt(PIN_BUZZER, 0);
}

void board_lte_power(bool on)
{
    gpio_set_level_opt(PIN_LTE_PWR_EN, on ? 1 : 0);
}

void board_cam_power(bool on)
{
    gpio_set_level_opt(PIN_CAM_PWR_EN, on ? 1 : 0);
}
