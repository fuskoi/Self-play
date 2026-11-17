#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/** Wi‑Fi MQTT 是否已连上并可发布 */
bool wifi_mqtt_is_ready(void);

/** 发布 telemetry JSON 到 device/{id}/telemetry */
esp_err_t wifi_mqtt_publish_telemetry(const char *json);

/** BLE 配网写入新凭证后调用，触发重连 */
void wifi_mqtt_notify_creds_changed(void);

/** 从 NVS 读/写 Wi‑Fi 凭证 */
esp_err_t wifi_creds_save(const char *ssid, const char *pass);
esp_err_t wifi_creds_load(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
bool      wifi_creds_present(void);
