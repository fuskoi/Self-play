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

#define APP_TOF_ZONE_N            64   /* 8x8 */
#define APP_MAX_TRACKS            8
#define APP_MAX_CLUSTERS          6

#define APP_W_SINGLE_ZONES        2.2f /* 单人簇宽度阈值（区） */
#define APP_MULTI_WIDTH_K         1.6f

#define APP_MQTT_HEARTBEAT_MS     30000
#define APP_UI_PERIOD_MS          100

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
    int32_t  occ;
    uint8_t  bat_percent;
    uint8_t  alarm;      /* bit0 超员 bit1 低电 */
    net_mode_t net_mode;
    sys_mode_t sys_mode;
    uint16_t occ_threshold;
    uint32_t idle_ms;
} app_state_t;

void app_state_init(void);
app_state_t *app_state_get(void);
void app_state_lock(void);
void app_state_unlock(void);