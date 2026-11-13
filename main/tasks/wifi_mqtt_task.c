#include "tasks_api.h"
#include "net_proto.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "wifi_mqtt";
static esp_mqtt_client_handle_t s_mqtt;

static void wifi_init_sta(const char *ssid, const char *pass)
{
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wcfg = {0};
    strncpy((char *)wcfg.sta.ssid, ssid, sizeof(wcfg.sta.ssid) - 1);
    strncpy((char *)wcfg.sta.password, pass, sizeof(wcfg.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void wifi_mqtt_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "start");

    /* 默认空凭证：未配网则不连，避免报错刷屏；配网后写入非易失存储的账号密码 */
    char ssid[33] = {0};
    char pass[65] = {0};
    nvs_handle_t h;
    if (nvs_open("ridership", NVS_READONLY, &h) == ESP_OK) {
        size_t sl = sizeof(ssid), pl = sizeof(pass);
        nvs_get_str(h, "wifi_ssid", ssid, &sl);
        nvs_get_str(h, "wifi_pass", pass, &pl);
        nvs_close(h);
    }

    if (ssid[0] == 0) {
        ESP_LOGW(TAG, "no wifi creds in NVS, idle");
        while (1) vTaskDelay(pdMS_TO_TICKS(5000));
    }

    wifi_init_sta(ssid, pass);
    /* 简化：延时后认为已连（量产应等待获取 IP 事件） */
    vTaskDelay(pdMS_TO_TICKS(5000));

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.emqx.io",
    };
    s_mqtt = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt) {
        esp_mqtt_client_start(s_mqtt);
        mqtt_route_set_wifi_ready(true);
        ESP_LOGI(TAG, "mqtt started");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}