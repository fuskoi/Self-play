#include "tasks_api.h"
#include "power_mgr.h"
#include "tof_api.h"
#include "app_config.h"
#include "drivers.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "power";

void power_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "start");
    while (1) {
        uint8_t pct = battery_percent();
        app_state_t *st = app_state_get();
        bool low_beep = false;
        app_state_lock();
        st->bat_percent = pct;
        if (battery_is_low()) {
            st->alarm |= 0x02;
            low_beep = true;
        } else if (battery_read_mv() > APP_VBAT_CLEAR_MV) {
            st->alarm &= (uint8_t)~0x02;
        }
        uint32_t idle_ms = st->idle_ms;
        sys_mode_t mode = st->sys_mode;
        app_state_unlock();

        if (low_beep && mode == SYS_MODE_ACTIVE) {
            board_buzzer_beep(2);
        }

        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
        uint32_t last = tof_last_detect_ms();
        if (mode == SYS_MODE_ACTIVE && last > 0 && (now - last) > idle_ms) {
            ESP_LOGI(TAG, "idle -> standby");
            standby_request_enter();
        }

        standby_service();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}