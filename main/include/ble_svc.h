#pragma once

#include <stdbool.h>
#include "esp_err.h"

/** GATT Notify 推送 telemetry JSON（连接且 CCC 开启时） */
esp_err_t ble_notify_telemetry(const char *json);

/** 是否有手机已连接 */
bool ble_is_connected(void);
