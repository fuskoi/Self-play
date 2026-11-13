#include "tasks_api.h"
#include "app_config.h"
#include "drivers.h"
#include "esp_log.h"

extern void ui_render_main(void);

static const char *TAG = "ui";

void ui_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "start");
    uint8_t page = 0;
    while (1) {
        if (board_key1_pressed()) {
            page ^= 1;
            vTaskDelay(pdMS_TO_TICKS(250));
        }
        if (board_key2_pressed()) {
            app_state_t *st = app_state_get();
            app_state_lock();
            st->net_mode = (net_mode_t)((st->net_mode + 1) % 3);
            app_state_unlock();
            xEventGroupSetBits(app_events(), EVT_TELEMETRY_PUSH);
            vTaskDelay(pdMS_TO_TICKS(250));
        }

        xEventGroupWaitBits(app_events(), EVT_UI_REFRESH, pdTRUE, pdFALSE, pdMS_TO_TICKS(APP_UI_PERIOD_MS));
        ui_render_main();
        (void)page;
    }
}