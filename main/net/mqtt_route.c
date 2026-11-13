#include "net_proto.h"
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
    if (s_ble && json) {
        ESP_LOGI(TAG, "BLE notify: %s", json);
        /* TODO: GATT Notify 在 ble_task 中真正发送 */
    }
}

void mqtt_route_publish(const char *json)
{
    if (!json) return;
    app_state_t *st = app_state_get();

    if (s_ble) {
        mqtt_route_notify_ble(json);
    }

    bool use_wifi = false, use_lte = false;
    app_state_lock();
    net_mode_t mode = st->net_mode;
    app_state_unlock();

    if (mode == NET_MODE_WIFI) use_wifi = s_wifi;
    else if (mode == NET_MODE_LTE) use_lte = s_lte;
    else {
        if (s_wifi) use_wifi = true;
        else if (s_lte) use_lte = true;
    }

    if (use_wifi) {
        ESP_LOGI(TAG, "WiFi MQTT pub: %s", json);
        /* 实际发布在 wifi_mqtt_task 订阅队列；此处先打日志，由任务侧扩展 */
    } else if (use_lte) {
        ESP_LOGI(TAG, "LTE MQTT pub: %s", json);
    } else {
        ESP_LOGW(TAG, "no uplink ready, drop: %s", json);
    }
}