#include "power_mgr.h"
#include "drivers.h"
#include "board_pins.h"
#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "bat";
static uint16_t s_sim_mv; /* 0=未模拟 */

void battery_set_sim_mv(uint16_t mv)
{
    s_sim_mv = mv;
    ESP_LOGI(TAG, "sim_mv=%u", (unsigned)mv);
}

esp_err_t battery_init(void)
{
    /* ADC 通道已在 board_io_init 配置 */
    int raw = 0;
    bool ok = board_adc_read(BAT_ADC_CHANNEL, &raw) == ESP_OK;
    ESP_LOGI(TAG, "battery adc %s (GPIO%d)", ok ? "ok" : "stub", PIN_BAT_ADC);
    return ESP_OK;
}

uint16_t battery_read_mv(void)
{
    if (s_sim_mv > 0) {
        return s_sim_mv;
    }
    int raw = 0;
    if (board_adc_read(BAT_ADC_CHANNEL, &raw) != ESP_OK) {
        return 3900;
    }
    /* 100k+100k 分压；现场可再标定 */
    int mv = raw * 2 * 3300 / 4095;
    if (mv < 3000) {
        mv = 3000;
    }
    if (mv > 4200) {
        mv = 4200;
    }
    return (uint16_t)mv;
}

uint8_t battery_percent(void)
{
    uint16_t mv = battery_read_mv();
    if (mv <= 3400) {
        return 0;
    }
    if (mv >= 4100) {
        return 100;
    }
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
