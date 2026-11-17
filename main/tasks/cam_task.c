#include "tasks_api.h"
#include "drivers.h"
#include "algo.h"
#include "esp_log.h"

static const char *TAG = "cam_task";

void cam_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "start");
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(app_events(), EVT_MULTI_SUSPECT,
                                               pdTRUE, pdFALSE, portMAX_DELAY);
        if (!(bits & EVT_MULTI_SUSPECT)) continue;

        uint8_t *gray = NULL;
        int w = 0, h = 0;
        if (app_cam_capture_gray(&gray, &w, &h) == ESP_OK) {
            int hint = algo_cam_count_people(gray, w, h);
            app_cam_release(gray);
            ESP_LOGI(TAG, "person_hint=%d", hint);
            if (hint >= 2) {
                count_msg_t msg = { .delta_in = 0, .delta_out = 0, .person_hint = hint };
                xQueueSend(count_queue(), &msg, 0);
            }
        }
    }
}