#include "tasks_api.h"
#include "app_config.h"
#include "drivers.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "count";

static void persist_counts(const app_state_t *st)
{
    nvs_handle_t h;
    if (nvs_open("ridership", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u32(h, "in", st->in_count);
    nvs_set_u32(h, "out", st->out_count);
    nvs_commit(h);
    nvs_close(h);
}

void count_task(void *arg)
{
    (void)arg;
    count_msg_t msg;
    ESP_LOGI(TAG, "start");
    while (1) {
        if (xQueueReceive(count_queue(), &msg, portMAX_DELAY) != pdTRUE) continue;

        app_state_t *st = app_state_get();
        app_state_lock();
        if (msg.delta_in > 0) st->in_count += (uint32_t)msg.delta_in;
        if (msg.delta_out > 0) st->out_count += (uint32_t)msg.delta_out;
        /* 人数提示：宽目标校正——一次穿越但相机认为多人时，补记进入 */
        if (msg.person_hint >= 2 && msg.delta_in == 1) {
            st->in_count += (uint32_t)(msg.person_hint - 1);
        }
        int64_t occ = (int64_t)st->in_count - (int64_t)st->out_count;
        if (occ < 0) occ = 0;
        st->occ = (int32_t)occ;

        if (st->occ >= st->occ_threshold) {
            st->alarm |= 0x01;
            app_state_unlock();
            board_buzzer_beep(1);
            app_state_lock();
        } else {
            st->alarm &= (uint8_t)~0x01;
        }
        uint32_t in_log = st->in_count;
        uint32_t out_log = st->out_count;
        int32_t occ_log = st->occ;
        persist_counts(st);
        app_state_unlock();

        xEventGroupSetBits(app_events(), EVT_UI_REFRESH | EVT_TELEMETRY_PUSH);
        ESP_LOGI(TAG, "in=%lu out=%lu occ=%ld",
                 (unsigned long)in_log, (unsigned long)out_log, (long)occ_log);
    }
}