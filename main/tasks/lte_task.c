#include "tasks_api.h"
#include "drivers.h"
#include "net_proto.h"
#include "wifi_mqtt.h"
#include "app_config.h"
#include "power_mgr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "lte";

typedef enum {
    LTE_ST_OFF = 0,
    LTE_ST_POWERING,
    LTE_ST_NET,
    LTE_ST_MQTT,
    LTE_ST_ONLINE,
    LTE_ST_BACKOFF,
} lte_state_t;

static lte_state_t s_st = LTE_ST_OFF;
static TickType_t s_backoff_until;

static bool lte_should_run(net_mode_t mode, bool wifi_up)
{
    if (mode == NET_MODE_LTE) {
        return true;
    }
    if (mode == NET_MODE_AUTO) {
        return !wifi_up; /* Wi‑Fi 优先，不可用时走 4G */
    }
    return false;
}

static void enter_off(void)
{
    if (s_st != LTE_ST_OFF) {
        lte_power_off();
    }
    mqtt_route_set_lte_ready(false);
    s_st = LTE_ST_OFF;
}

void lte_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "start");
    lte_init();
    s_backoff_until = 0;

    char client_id[40];
    snprintf(client_id, sizeof(client_id), "%s-%02X", APP_DEVICE_ID_PREFIX,
             (unsigned)(xTaskGetTickCount() & 0xFF));

    while (1) {
        app_state_t *st = app_state_get();
        app_state_lock();
        net_mode_t mode = st->net_mode;
        sys_mode_t sys = st->sys_mode;
        app_state_unlock();

        const bool crit = battery_is_crit();
        const bool wifi_up = wifi_mqtt_is_ready();
        const bool want = !crit && sys == SYS_MODE_ACTIVE && lte_should_run(mode, wifi_up);

        if (!want) {
            enter_off();
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        lte_poll();

        switch (s_st) {
        case LTE_ST_OFF:
            s_st = LTE_ST_POWERING;
            break;

        case LTE_ST_POWERING:
            if (lte_power_on_sequence() == ESP_OK) {
                s_st = LTE_ST_NET;
            } else {
                ESP_LOGW(TAG, "power-on fail, backoff");
                s_st = LTE_ST_BACKOFF;
                s_backoff_until = xTaskGetTickCount() + pdMS_TO_TICKS(APP_LTE_RECONNECT_MS);
            }
            break;

        case LTE_ST_NET:
            if (lte_wait_network(APP_LTE_NET_TIMEOUT_MS) == ESP_OK) {
                s_st = LTE_ST_MQTT;
            } else {
                s_st = LTE_ST_BACKOFF;
                s_backoff_until = xTaskGetTickCount() + pdMS_TO_TICKS(APP_LTE_RECONNECT_MS);
            }
            break;

        case LTE_ST_MQTT:
            if (lte_mqtt_connect(APP_LTE_MQTT_HOST, APP_LTE_MQTT_PORT, client_id) == ESP_OK) {
                mqtt_route_set_lte_ready(true);
                s_st = LTE_ST_ONLINE;
                ESP_LOGI(TAG, "online");
            } else {
                mqtt_route_set_lte_ready(false);
                s_st = LTE_ST_BACKOFF;
                s_backoff_until = xTaskGetTickCount() + pdMS_TO_TICKS(APP_LTE_RECONNECT_MS);
            }
            break;

        case LTE_ST_ONLINE:
            if (!lte_mqtt_is_ready()) {
                ESP_LOGW(TAG, "mqtt dropped");
                mqtt_route_set_lte_ready(false);
                s_st = LTE_ST_BACKOFF;
                s_backoff_until = xTaskGetTickCount() + pdMS_TO_TICKS(APP_LTE_RECONNECT_MS);
            }
            break;

        case LTE_ST_BACKOFF:
            mqtt_route_set_lte_ready(false);
            if ((int32_t)(xTaskGetTickCount() - s_backoff_until) >= 0) {
                /* 全量重来：断电再上电更干净 */
                lte_power_off();
                s_st = LTE_ST_POWERING;
            }
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(s_st == LTE_ST_ONLINE ? 500 : 200));
    }
}
