#include "tasks_api.h"
#include "drivers.h"
#include "algo.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "tof_api.h"
#include <string.h>

static const char *TAG = "tof_task";
static track_t s_tracks[APP_MAX_TRACKS];
static uint32_t s_last_detect_ms;
static uint32_t s_pending_hint;
static uint32_t s_pending_hint_ms;
static bool s_calibrating;
static int s_cal_frames;

uint32_t tof_last_detect_ms(void)
{
    return s_last_detect_ms;
}

void tof_set_pending_person_hint(int hint)
{
    if (hint < 1) {
        return;
    }
    s_pending_hint = (uint32_t)hint;
    s_pending_hint_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
}

int tof_take_pending_person_hint(uint32_t now_ms)
{
    if (s_pending_hint == 0) {
        return 0;
    }
    if (now_ms - s_pending_hint_ms > APP_CAM_HINT_WINDOW_MS) {
        s_pending_hint = 0;
        return 0;
    }
    int h = (int)s_pending_hint;
    s_pending_hint = 0;
    return h;
}

void tof_start_bg_calibrate(void)
{
    algo_bg_reset();
    s_calibrating = true;
    s_cal_frames = 0;
    ESP_LOGI(TAG, "bg calibrate start");
}

bool tof_bg_calibrating(void)
{
    return s_calibrating;
}

void tof_task(void *arg)
{
    (void)arg;
    uint16_t a[APP_TOF_ZONE_N], b[APP_TOF_ZONE_N], fused[APP_TOF_ZONE_N];
    perceive_result_t perc;
    memset(s_tracks, 0, sizeof(s_tracks));

    ESP_LOGI(TAG, "start");
    while (1) {
        app_state_t *st = app_state_get();
        app_state_lock();
        sys_mode_t mode = st->sys_mode;
        float install_h = st->install_h_m;
        float gate_y = st->gate_y;
        app_state_unlock();

        if (mode == SYS_MODE_STANDBY) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        if (tof_read_both(a, b) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        float pa = 0, pb = 0;
        algo_fuse_dual_depth(a, b, fused, &pa, &pb);

        if (s_calibrating) {
            algo_bg_accumulate(fused);
            s_cal_frames++;
            if (s_cal_frames >= 20) {
                bool ok = algo_bg_finalize();
                s_calibrating = false;
                app_state_lock();
                st->bg_calibrated = ok;
                app_state_unlock();
                ESP_LOGI(TAG, "bg calibrate %s (%d frames)", ok ? "ok" : "fail", s_cal_frames);
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        algo_cluster_from_depth(fused, install_h, pa, pb, &perc);
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);

        if (perc.n_clusters > 0 || algo_count_human_zones(fused, install_h) > 0) {
            s_last_detect_ms = now;
        }

        /* 尾随：已有 track 又新增簇 */
        int active_before = 0;
        for (int i = 0; i < APP_MAX_TRACKS; i++) {
            if (s_tracks[i].active) {
                active_before++;
            }
        }

        algo_tracker_update(&perc, now, s_tracks);

        int active_after = 0;
        for (int i = 0; i < APP_MAX_TRACKS; i++) {
            if (s_tracks[i].active) {
                active_after++;
            }
        }
        if (active_after > active_before && active_before >= 1) {
            perc.multi_suspect = true;
        }

        cross_event_t ev = algo_line_cross_process(s_tracks, gate_y, now);

        if (perc.multi_suspect) {
            xEventGroupSetBits(app_events(), EVT_MULTI_SUSPECT);
        }

        if (ev.delta_in || ev.delta_out) {
            int hint = tof_take_pending_person_hint(now);
            count_msg_t msg = {
                .delta_in = ev.delta_in,
                .delta_out = ev.delta_out,
                .person_hint = hint,
            };
            xQueueSend(count_queue(), &msg, 0);
            xEventGroupSetBits(app_events(), EVT_CROSS_UPDATE | EVT_TELEMETRY_PUSH | EVT_UI_REFRESH);
        }

        vTaskDelay(pdMS_TO_TICKS(1000 / APP_TOF_HZ_ACTIVE));
    }
}
