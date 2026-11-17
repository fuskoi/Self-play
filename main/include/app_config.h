#pragma once

#include <stdint.h>
#include <stdbool.h>

#define APP_DEVICE_ID_PREFIX      "pcounter"
#define APP_T_IDLE_MS             15000
#define APP_TOF_HZ_ACTIVE         20
#define APP_OCC_THRESHOLD_DEFAULT 50

#define APP_VBAT_LOW_MV           3500
#define APP_VBAT_CRIT_MV          3400
#define APP_VBAT_CLEAR_MV         3600
#define APP_LOWBAT_BEEP_MS        30000

#define APP_TOF_ZONE_N            64   /* 8×8 分区 */
#define APP_MAX_TRACKS            8
#define APP_MAX_CLUSTERS          6

/* 俯视人体高度带（安装高度 − 测距） */
#define APP_HUMAN_H_MIN_M         0.85f
#define APP_HUMAN_H_MAX_M         2.15f
#define APP_INSTALL_H_M_DEFAULT   2.30f
#define APP_GATE_Y_DEFAULT        3.5f   /* 8×8 网格门线纵坐标 */

#define APP_W_SINGLE_ZONES        2.2f /* 单人簇宽度阈值（分区数） */
#define APP_MULTI_WIDTH_K         1.6f
#define APP_TRACK_TIMEOUT_MS      500
#define APP_CROSS_COOLDOWN_MS     800  /* 同一 track 过线防抖 */
#define APP_SIDE_STABLE_MS        80   /* 侧别确认最小停留 */
#define APP_CAM_HINT_WINDOW_MS    1500 /* 相机人数提示有效窗 */
#define APP_OCC_CLEAR_HYST        2    /* 超员解除滞回 */

#define APP_MQTT_HEARTBEAT_MS     30000
#define APP_UI_PERIOD_MS          100

/* Wi‑Fi MQTT（与方案 §6.3 Topic 一致） */
#define APP_WIFI_MQTT_URI           "mqtt://broker.emqx.io"
#define APP_WIFI_MQTT_PORT          1883

/* Air780E / 银尔达 M100M AT 固件 MQTT */
#define APP_LTE_MQTT_HOST           "broker.emqx.io"
#define APP_LTE_MQTT_PORT           1883
#define APP_LTE_MQTT_USER           ""
#define APP_LTE_MQTT_PASS           ""
#define APP_LTE_MQTT_KEEPALIVE_S    60
#define APP_LTE_MQTT_TOPIC_TELEMETRY "ridership/telemetry"
#define APP_LTE_MQTT_TOPIC_CMD       "ridership/cmd"
#define APP_LTE_NET_TIMEOUT_MS      90000
#define APP_LTE_MQTT_CONN_TIMEOUT_MS 30000
#define APP_LTE_RECONNECT_MS        10000

typedef enum {
    NET_MODE_WIFI = 0,
    NET_MODE_LTE  = 1,
    NET_MODE_AUTO = 2,
} net_mode_t;

typedef enum {
    SYS_MODE_ACTIVE = 0,
    SYS_MODE_STANDBY = 1,
} sys_mode_t;

typedef struct {
    uint32_t in_count;
    uint32_t out_count;
    int32_t  occ;            /* 在场 = in - out（下限 0） */
    uint8_t  bat_percent;
    uint8_t  alarm;          /* bit0 超员；bit1 低电 */
    net_mode_t net_mode;
    sys_mode_t sys_mode;
    uint16_t occ_threshold;
    uint32_t idle_ms;
    float    install_h_m;    /* 安装高度，标定用 */
    float    gate_y;         /* 虚拟门线（网格 Y） */
    bool     bg_calibrated;
} app_state_t;

void app_state_init(void);
app_state_t *app_state_get(void);
void app_state_lock(void);
void app_state_unlock(void);
void app_state_load_nvs(void);
void app_state_persist_counts(void);
void app_state_persist_params(void);
