#include "tasks_api.h"
#include "app_config.h"
#include "drivers.h"
#include "esp_log.h"

static const char *TAG = "count";

void count_task(void *arg)
{
    (void)arg;
    count_msg_t msg;
    ESP_LOGI(TAG, "start");
    while (1) {
        if (xQueueReceive(count_queue(), &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        app_state_t *st = app_state_get();
        app_state_lock();

        int din = msg.delta_in;
        int dout = msg.delta_out;

        /* 宽目标/并行：相机认为多人且本帧只计了 1 次进/出时补记 */
        if (msg.person_hint >= 2) {
            if (din == 1) {
                din += (msg.person_hint - 1);
            } else if (dout == 1) {
                dout += (msg.person_hint - 1);
            }
        }

        if (din > 0) {
            st->in_count += (uint32_t)din;
        }
        if (dout > 0) {
            st->out_count += (uint32_t)dout;
        }

        int64_t occ = (int64_t)st->in_count - (int64_t)st->out_count;
        if (occ < 0) {
            occ = 0;
        }
        st->occ = (int32_t)occ;

        bool over = false;
        if (st->occ >= (int32_t)st->occ_threshold) {
            st->alarm |= 0x01;
            over = true;
        } else if (st->occ + APP_OCC_CLEAR_HYST < (int32_t)st->occ_threshold) {
            st->alarm &= (uint8_t)~0x01;
        }

        uint32_t in_log = st->in_count;
        uint32_t out_log = st->out_count;
        int32_t occ_log = st->occ;
        app_state_persist_counts();
        app_state_unlock();

        if (over) {
            board_buzzer_beep(1);
        }

        xEventGroupSetBits(app_events(), EVT_UI_REFRESH | EVT_TELEMETRY_PUSH);
        ESP_LOGI(TAG, "in=%lu out=%lu occ=%ld hint=%d",
                 (unsigned long)in_log, (unsigned long)out_log, (long)occ_log,
                 msg.person_hint);
    }
}
