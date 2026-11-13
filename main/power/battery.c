#include "power_mgr.h"
#include "board_pins.h"
#include "app_config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "bat";
static adc_oneshot_unit_handle_t s_adc;
static bool s_ok;

esp_err_t battery_init(void)
{
    adc_oneshot_unit_init_cfg_t cfg = { .unit_id = ADC_UNIT_1 };
    esp_err_t err = adc_oneshot_new_unit(&cfg, &s_adc);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "adc init fail, use stub");
        return ESP_OK;
    }
    adc_oneshot_chan_cfg_t ch = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    /* PIN_BAT_ADC=GPIO2 → 需按 S3 ADC 通道映射修改 */
    err = adc_oneshot_config_channel(s_adc, ADC_CHANNEL_1, &ch);
    s_ok = (err == ESP_OK);
    ESP_LOGI(TAG, "battery adc %s", s_ok ? "ok" : "stub");
    return ESP_OK;
}

uint16_t battery_read_mv(void)
{
    if (!s_ok) return 3900;
    int raw = 0;
    adc_oneshot_read(s_adc, ADC_CHANNEL_1, &raw);
    /* 假设 1:1 分压后校准：raw -> mV，现场标定 */
    int mv = raw * 2 * 3300 / 4095;
    if (mv < 3000) mv = 3000;
    if (mv > 4200) mv = 4200;
    return (uint16_t)mv;
}

uint8_t battery_percent(void)
{
    uint16_t mv = battery_read_mv();
    if (mv <= 3400) return 0;
    if (mv >= 4100) return 100;
    return (uint8_t)((mv - 3400) * 100 / 700);
}

bool battery_is_low(void)
{
    return battery_read_mv() < APP_VBAT_LOW_MV;
}

bool battery_is_crit(void)
{
    return battery_read_mv() < APP_VBAT_CRIT_MV;
}