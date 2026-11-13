#include "drivers.h"
#include "board_pins.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "oled";
static bool s_on = true;

/* 精简桩：无屏时用日志代替；可替换为 u8g2 / esp_lcd SSD1306 */

esp_err_t oled_init(void)
{
    ESP_LOGI(TAG, "SSD1306 init (log fallback)");
    return ESP_OK;
}

void oled_clear(void) {}

void oled_power(bool on)
{
    s_on = on;
}

void oled_show_main(uint32_t in_c, uint32_t out_c, int32_t occ)
{
    if (!s_on) return;
    ESP_LOGI(TAG, "IN=%lu OUT=%lu OCC=%ld", (unsigned long)in_c, (unsigned long)out_c, (long)occ);
}

void oled_show_status(uint8_t bat, const char *net, uint8_t alarm)
{
    if (!s_on) return;
    ESP_LOGI(TAG, "BAT=%u%% NET=%s ALARM=0x%02X", bat, net ? net : "?", alarm);
}