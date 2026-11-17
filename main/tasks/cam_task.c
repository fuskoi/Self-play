#include "tasks_api.h"
#include "drivers.h"
#include "algo.h"
#include "tof_api.h"
#include "esp_log.h"

static const char *TAG = "cam_task";

void cam_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "start");
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(app_events(), EVT_MULTI_SUSPECT,
                                               pdTRUE, pdFALSE, portMAX_DELAY);
        if (!(bits & EVT_MULTI_SUSPECT)) {
            continue;
        }

        uint8_t *gray = NULL;
        int w = 0, h = 0;
        if (app_cam_capture_gray(&gray, &w, &h) != ESP_OK) {
            continue;
        }
        int hint = algo_cam_count_people(gray, w, h);
        app_cam_release(gray);
        ESP_LOGI(TAG, "person_hint=%d", hint);

        /* 写入过线校正窗口，由 tof_task 在真正过线时消费 */
        if (hint >= 2) {
            tof_set_pending_person_hint(hint);
        }
    }
}
