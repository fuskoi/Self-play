#pragma once

#include "app_config.h"

#define TELEMETRY_JSON_MAX 256
#define CMD_JSON_MAX       192

void proto_device_id(char *buf, int buflen);
int  proto_build_telemetry(const app_state_t *st, char *buf, int buflen);
bool proto_handle_command(const char *json, app_state_t *st);

void mqtt_route_init(void);
void mqtt_route_publish(const char *json);
void mqtt_route_set_wifi_ready(bool ready);
void mqtt_route_set_lte_ready(bool ready);
void mqtt_route_set_ble_connected(bool connected);
void mqtt_route_notify_ble(const char *json); /* 由蓝牙任务实现弱回调或直接调用 */