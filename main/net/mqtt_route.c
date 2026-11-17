#include "net_proto.h"
#include "drivers.h"
#include "wifi_mqtt.h"
#include "ble_svc.h"
#include "app_config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "route";
static bool s_wifi, s_lte, s_ble;

void mqtt_route_init(void)
{
    s_wifi = s_lte = s_ble = false;
}

void mqtt_route_set_wifi_ready(bool ready) { s_wifi = ready; }
void mqtt_route_set_lte_ready(bool ready) { s_lte = ready; }
void mqtt_route_set_ble_connected(bool connected) { s_ble = connected; }

void mqtt_route_notify_ble(const char *json)
{
    if (!json) {
        return;
    }
    if (ble_notify_telemetry(json) != ESP_OK) {
        ESP_LOGD(TAG, "BLE notify skip");
    }
}

void mqtt_route_publish(const char *json)
{
    if (!json) {
        return;
    }
    app_state_t *st = app_state_get();

    if (s_ble) {
        mqtt_route_notify_ble(json);
    }

    bool use_wifi = false, use_lte = false;
    app_state_lock();
    net_mode_t mode = st->net_mode;
    app_state_unlock();

    if (mode == NET_MODE_WIFI) {
        use_wifi = s_wifi;
    } else if (mode == NET_MODE_LTE) {
        use_lte = s_lte;
    } else {
        if (s_wifi) {
            use_wifi = true;
        } else if (s_lte) {
            use_lte = true;
        }
    }

    if (use_wifi) {
        if (wifi_mqtt_publish_telemetry(json) != ESP_OK) {
            ESP_LOGW(TAG, "WiFi MQTT pub failed");
            s_wifi = false;
        }
    } else if (use_lte) {
        if (lte_mqtt_publish(APP_LTE_MQTT_TOPIC_TELEMETRY, json) != ESP_OK) {
            ESP_LOGW(TAG, "LTE MQTT pub failed");
            s_lte = false;
        }
    } else if (!s_ble) {
        ESP_LOGD(TAG, "no uplink ready");
    }
}
