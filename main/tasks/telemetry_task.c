#include "tasks_api.h"
#include "net_proto.h"
#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "telem";

void telemetry_task(void *arg)
{
    (void)arg;
    char buf[TELEMETRY_JSON_MAX];
    ESP_LOGI(TAG, "start");
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(app_events(), EVT_TELEMETRY_PUSH,
                                               pdTRUE, pdFALSE, pdMS_TO_TICKS(APP_MQTT_HEARTBEAT_MS));
        (void)bits;
        app_state_t *st = app_state_get();
        app_state_lock();
        proto_build_telemetry(st, buf, sizeof(buf));
        app_state_unlock();
        mqtt_route_publish(buf);
    }
}