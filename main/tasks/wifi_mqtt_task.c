#include "tasks_api.h"
#include "net_proto.h"
#include "wifi_mqtt.h"
#include "app_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "nvs.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "wifi_mqtt";

#define WIFI_GOT_IP_BIT      BIT0
#define WIFI_FAIL_BIT        BIT1
#define MQTT_CONNECTED_BIT   BIT2

static EventGroupHandle_t s_wifi_eg;
static esp_mqtt_client_handle_t s_mqtt;
static bool s_wifi_started;
static bool s_netif_created;
static char s_dev_id[24];
static char s_topic_tele[64];
static char s_topic_cmd[64];

esp_err_t wifi_creds_save(const char *ssid, const char *pass)
{
    if (!ssid || !ssid[0]) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open("ridership", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(h, "wifi_ssid", ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(h, "wifi_pass", pass ? pass : "");
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t wifi_creds_load(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    if (!ssid || ssid_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    ssid[0] = 0;
    if (pass && pass_len) {
        pass[0] = 0;
    }
    nvs_handle_t h;
    if (nvs_open("ridership", NVS_READONLY, &h) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    size_t sl = ssid_len, pl = pass_len;
    esp_err_t e1 = nvs_get_str(h, "wifi_ssid", ssid, &sl);
    esp_err_t e2 = ESP_OK;
    if (pass && pass_len) {
        e2 = nvs_get_str(h, "wifi_pass", pass, &pl);
        if (e2 == ESP_ERR_NVS_NOT_FOUND) {
            pass[0] = 0;
            e2 = ESP_OK;
        }
    }
    nvs_close(h);
    return (e1 == ESP_OK && e2 == ESP_OK) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

bool wifi_creds_present(void)
{
    char ssid[33] = {0};
    return wifi_creds_load(ssid, sizeof(ssid), NULL, 0) == ESP_OK && ssid[0] != 0;
}

void wifi_mqtt_notify_creds_changed(void)
{
    xEventGroupSetBits(app_events(), EVT_WIFI_CREDS);
}

bool wifi_mqtt_is_ready(void)
{
    if (!s_wifi_eg || !s_mqtt) {
        return false;
    }
    return (xEventGroupGetBits(s_wifi_eg) & MQTT_CONNECTED_BIT) != 0;
}

esp_err_t wifi_mqtt_publish_telemetry(const char *json)
{
    if (!json || !wifi_mqtt_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    int msg_id = esp_mqtt_client_publish(s_mqtt, s_topic_tele, json, 0, 0, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

static void rebuild_topics(void)
{
    proto_device_id(s_dev_id, sizeof(s_dev_id));
    snprintf(s_topic_tele, sizeof(s_topic_tele), "device/%s/telemetry", s_dev_id);
    snprintf(s_topic_cmd, sizeof(s_topic_cmd), "device/%s/cmd", s_dev_id);
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "wifi disconnected, retry");
        xEventGroupClearBits(s_wifi_eg, WIFI_GOT_IP_BIT | MQTT_CONNECTED_BIT);
        mqtt_route_set_wifi_ready(false);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "got ip " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_eg, WIFI_GOT_IP_BIT);
    }
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    esp_mqtt_event_handle_t ev = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "mqtt connected, sub %s", s_topic_cmd);
        esp_mqtt_client_subscribe(s_mqtt, s_topic_cmd, 0);
        xEventGroupSetBits(s_wifi_eg, MQTT_CONNECTED_BIT);
        mqtt_route_set_wifi_ready(true);
        xEventGroupSetBits(app_events(), EVT_TELEMETRY_PUSH);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "mqtt disconnected");
        xEventGroupClearBits(s_wifi_eg, MQTT_CONNECTED_BIT);
        mqtt_route_set_wifi_ready(false);
        break;
    case MQTT_EVENT_DATA:
        if (ev->data_len > 0 && ev->data_len < CMD_JSON_MAX) {
            char cmd[CMD_JSON_MAX];
            int n = ev->data_len < (int)sizeof(cmd) - 1 ? ev->data_len : (int)sizeof(cmd) - 1;
            memcpy(cmd, ev->data, n);
            cmd[n] = 0;
            ESP_LOGI(TAG, "cmd: %s", cmd);
            app_state_t *st = app_state_get();
            app_state_lock();
            proto_handle_command(cmd, st);
            app_state_unlock();
            xEventGroupSetBits(app_events(), EVT_UI_REFRESH | EVT_TELEMETRY_PUSH);
        }
        break;
    default:
        break;
    }
}

static esp_err_t wifi_start_sta(const char *ssid, const char *pass)
{
    if (!s_netif_created) {
        esp_netif_create_default_wifi_sta();
        s_netif_created = true;
    }
    if (!s_wifi_started) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        s_wifi_started = true;
    }

    wifi_config_t wcfg = {0};
    strncpy((char *)wcfg.sta.ssid, ssid, sizeof(wcfg.sta.ssid) - 1);
    if (pass) {
        strncpy((char *)wcfg.sta.password, pass, sizeof(wcfg.sta.password) - 1);
    }
    wcfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    return ESP_OK;
}

static void mqtt_ensure_started(void)
{
    rebuild_topics();
    if (s_mqtt) {
        esp_mqtt_client_reconnect(s_mqtt);
        return;
    }
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = APP_WIFI_MQTT_URI,
        .credentials.client_id = s_dev_id,
    };
    s_mqtt = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt) {
        ESP_LOGE(TAG, "mqtt init fail");
        return;
    }
    esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt);
}

void wifi_mqtt_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "start");
    s_wifi_eg = xEventGroupCreate();
    rebuild_topics();

    char ssid[33] = {0};
    char pass[65] = {0};

    while (1) {
        app_state_t *st = app_state_get();
        app_state_lock();
        sys_mode_t sys = st->sys_mode;
        net_mode_t mode = st->net_mode;
        app_state_unlock();

        if (sys == SYS_MODE_STANDBY || mode == NET_MODE_LTE) {
            mqtt_route_set_wifi_ready(false);
            if (s_mqtt) {
                esp_mqtt_client_stop(s_mqtt);
            }
            if (s_wifi_started) {
                esp_wifi_stop();
            }
            xEventGroupClearBits(s_wifi_eg, WIFI_GOT_IP_BIT | MQTT_CONNECTED_BIT);
            /* 等退出待机或模式变化 / 新凭证 */
            xEventGroupWaitBits(app_events(), EVT_EXIT_STANDBY | EVT_WIFI_CREDS,
                                pdTRUE, pdFALSE, pdMS_TO_TICKS(2000));
            continue;
        }

        if (!wifi_creds_present()) {
            ESP_LOGW(TAG, "no wifi creds — wait BLE provision");
            mqtt_route_set_wifi_ready(false);
            xEventGroupWaitBits(app_events(), EVT_WIFI_CREDS, pdTRUE, pdFALSE, portMAX_DELAY);
            continue;
        }

        wifi_creds_load(ssid, sizeof(ssid), pass, sizeof(pass));
        ESP_LOGI(TAG, "connecting SSID=%s", ssid);
        xEventGroupClearBits(s_wifi_eg, WIFI_GOT_IP_BIT | MQTT_CONNECTED_BIT | WIFI_FAIL_BIT);
        wifi_start_sta(ssid, pass);

        EventBits_t bits = xEventGroupWaitBits(s_wifi_eg, WIFI_GOT_IP_BIT, pdFALSE, pdFALSE,
                                               pdMS_TO_TICKS(30000));
        if (!(bits & WIFI_GOT_IP_BIT)) {
            ESP_LOGW(TAG, "got ip timeout");
            vTaskDelay(pdMS_TO_TICKS(5000));
            /* 配网更新则立即换号 */
            if (xEventGroupGetBits(app_events()) & EVT_WIFI_CREDS) {
                xEventGroupClearBits(app_events(), EVT_WIFI_CREDS);
            }
            continue;
        }

        mqtt_ensure_started();
        bits = xEventGroupWaitBits(s_wifi_eg, MQTT_CONNECTED_BIT, pdFALSE, pdFALSE,
                                   pdMS_TO_TICKS(20000));
        if (!(bits & MQTT_CONNECTED_BIT)) {
            ESP_LOGW(TAG, "mqtt connect timeout");
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        ESP_LOGI(TAG, "online tele=%s", s_topic_tele);

        /* 在线：等待掉线、新凭证或待机 */
        while (wifi_mqtt_is_ready()) {
            EventBits_t ev = xEventGroupWaitBits(app_events(),
                                                 EVT_WIFI_CREDS | EVT_ENTER_STANDBY,
                                                 pdTRUE, pdFALSE, pdMS_TO_TICKS(2000));
            if (ev & EVT_WIFI_CREDS) {
                ESP_LOGI(TAG, "creds updated, reconnect");
                break;
            }
            if (ev & EVT_ENTER_STANDBY) {
                break;
            }
            app_state_lock();
            sys = st->sys_mode;
            mode = st->net_mode;
            app_state_unlock();
            if (sys == SYS_MODE_STANDBY || mode == NET_MODE_LTE) {
                break;
            }
        }

        mqtt_route_set_wifi_ready(false);
        xEventGroupClearBits(s_wifi_eg, MQTT_CONNECTED_BIT);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
