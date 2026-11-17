#include "tasks_api.h"
#include "app_config.h"
#include "drivers.h"
#include "net_proto.h"
#include "esp_log.h"
#include "esp_timer.h"

extern void ui_render_main(void);

static const char *TAG = "ui";

void ui_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "start");
    uint8_t page = 0;
    uint32_t key1_down_ms = 0;
    bool key1_long_done = false;

    while (1) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);

        /* KEY1：短按切页；长按 2s 提示配网（蜂鸣）；长按 5s 清零 */
        if (board_key1_pressed()) {
            if (key1_down_ms == 0) {
                key1_down_ms = now;
                key1_long_done = false;
            } else {
                uint32_t held = now - key1_down_ms;
                if (!key1_long_done && held >= 5000) {
                    app_state_t *st = app_state_get();
                    app_state_lock();
                    proto_handle_command("{\"cmd\":\"clear_daily\"}", st);
                    app_state_persist_counts();
                    app_state_unlock();
                    board_buzzer_beep(2);
                    key1_long_done = true;
                    xEventGroupSetBits(app_events(), EVT_UI_REFRESH | EVT_TELEMETRY_PUSH);
                    ESP_LOGI(TAG, "long press clear_daily");
                } else if (!key1_long_done && held >= 2000 && held < 2100) {
                    board_buzzer_beep(1);
                    ESP_LOGI(TAG, "BLE provision hint (hold phone near)");
                }
            }
        } else {
            if (key1_down_ms != 0 && !key1_long_done) {
                uint32_t held = now - key1_down_ms;
                if (held < 800) {
                    page ^= 1;
                    xEventGroupSetBits(app_events(), EVT_UI_REFRESH);
                }
            }
            key1_down_ms = 0;
            key1_long_done = false;
        }

        /* KEY2：短按切换 wifi/lte/auto */
        if (board_key2_pressed()) {
            app_state_t *st = app_state_get();
            app_state_lock();
            st->net_mode = (net_mode_t)((st->net_mode + 1) % 3);
            app_state_persist_params();
            app_state_unlock();
            xEventGroupSetBits(app_events(), EVT_TELEMETRY_PUSH | EVT_UI_REFRESH);
            vTaskDelay(pdMS_TO_TICKS(280));
        }

        xEventGroupWaitBits(app_events(), EVT_UI_REFRESH, pdTRUE, pdFALSE,
                            pdMS_TO_TICKS(APP_UI_PERIOD_MS));
        if (page == 0) {
            ui_render_main();
        } else {
            /* 状态页：复用 status 行，主区显示阈值/待机 */
            app_state_t *st = app_state_get();
            app_state_lock();
            uint32_t thr = st->occ_threshold;
            uint32_t idle_s = st->idle_ms / 1000;
            uint8_t bat = st->bat_percent;
            uint8_t alarm = st->alarm;
            net_mode_t nm = st->net_mode;
            app_state_unlock();
            const char *net = (nm == NET_MODE_WIFI) ? "wifi" :
                              (nm == NET_MODE_LTE) ? "lte" : "auto";
            oled_show_main(thr, idle_s, (int32_t)bat);
            oled_show_status(bat, net, alarm);
        }
    }
}
