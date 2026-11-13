#include "tasks_api.h"
#include "drivers.h"
#include "algo.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "tof_api.h"

static const char *TAG = "tof_task";
static track_t s_tracks[APP_MAX_TRACKS];
static uint32_t s_last_detect_ms;

uint32_t tof_last_detect_ms(void)
{
    return s_last_detect_ms;
}

void tof_task(void *arg)
{
    (void)arg;
    uint16_t a[APP_TOF_ZONE_N], b[APP_TOF_ZONE_N];
    perceive_result_t perc;
    const float install_h = 2.2f;
    const float gate_y = 3.5f;

    ESP_LOGI(TAG, "start");
    while (1) {
        app_state_t *st = app_state_get();
        app_state_lock();
        sys_mode_t mode = st->sys_mode;
        app_state_unlock();
        if (mode == SYS_MODE_STANDBY) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        if (tof_read_both(a, b) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        /* 融合：简单取 A 为主深度图（B 可用于方向增强，后续扩展） */
        algo_cluster_from_depth(a, install_h, &perc);
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
        if (perc.n_clusters > 0) {
            s_last_detect_ms = now;
        }

        algo_tracker_update(&perc, now, s_tracks);
        cross_event_t ev = algo_line_cross_process(s_tracks, gate_y);

        if (perc.multi_suspect) {
            xEventGroupSetBits(app_events(), EVT_MULTI_SUSPECT);
        }

        if (ev.delta_in || ev.delta_out) {
            count_msg_t msg = {
                .delta_in = ev.delta_in,
                .delta_out = ev.delta_out,
                .person_hint = 0,
            };
            xQueueSend(count_queue(), &msg, 0);
            xEventGroupSetBits(app_events(), EVT_CROSS_UPDATE | EVT_TELEMETRY_PUSH | EVT_UI_REFRESH);
        }

        vTaskDelay(pdMS_TO_TICKS(1000 / APP_TOF_HZ_ACTIVE));
    }
}