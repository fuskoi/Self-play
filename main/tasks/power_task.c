#include "tasks_api.h"
#include "power_mgr.h"
#include "tof_api.h"
#include "app_config.h"
#include "drivers.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_wifi.h"

static const char *TAG = "power";

void power_task(void *arg)
{
    (void)arg;
    uint32_t last_low_beep = 0;
    uint32_t boot_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    ESP_LOGI(TAG, "start");

    while (1) {
        uint8_t pct = battery_percent();
        app_state_t *st = app_state_get();
        bool low_beep = false;
        bool crit = false;

        app_state_lock();
        st->bat_percent = pct;
        if (battery_is_low()) {
            st->alarm |= 0x02;
            low_beep = true;
        } else if (battery_read_mv() > APP_VBAT_CLEAR_MV) {
            st->alarm &= (uint8_t)~0x02;
        }
        crit = battery_is_crit();
        uint32_t idle_ms = st->idle_ms;
        sys_mode_t mode = st->sys_mode;
        app_state_unlock();

        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
        if (low_beep && mode == SYS_MODE_ACTIVE) {
            if (last_low_beep == 0 || now - last_low_beep > APP_LOWBAT_BEEP_MS) {
                board_buzzer_beep(2);
                last_low_beep = now;
                xEventGroupSetBits(app_events(), EVT_TELEMETRY_PUSH | EVT_UI_REFRESH);
            }
        }

        if (crit && mode == SYS_MODE_ACTIVE) {
            ESP_LOGW(TAG, "battery critical -> standby");
            standby_request_enter();
        }

        /* 无人超时：从未检测到人时，从开机时刻起算 */
        uint32_t last = tof_last_detect_ms();
        uint32_t ref = (last > 0) ? last : boot_ms;
        if (mode == SYS_MODE_ACTIVE && !tof_bg_calibrating() &&
            (now - ref) > idle_ms) {
            ESP_LOGI(TAG, "idle -> standby");
            standby_request_enter();
        }

        standby_service();

        app_state_lock();
        mode = st->sys_mode;
        app_state_unlock();
        if (mode == SYS_MODE_STANDBY) {
            esp_wifi_stop();
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
