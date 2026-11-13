#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#define EVT_MULTI_SUSPECT   BIT0
#define EVT_CROSS_UPDATE    BIT1
#define EVT_TELEMETRY_PUSH  BIT2
#define EVT_UI_REFRESH      BIT3
#define EVT_ENTER_STANDBY   BIT4
#define EVT_EXIT_STANDBY    BIT5

EventGroupHandle_t app_events(void);
QueueHandle_t      count_queue(void);

typedef struct {
    int delta_in;
    int delta_out;
    int person_hint; /* 0 表示不用相机校正 */
} count_msg_t;

void tasks_start_all(void);

void tof_task(void *arg);
void cam_task(void *arg);
void count_task(void *arg);
void ui_task(void *arg);
void ble_task(void *arg);
void wifi_mqtt_task(void *arg);
void lte_task(void *arg);
void power_task(void *arg);
void telemetry_task(void *arg);
void console_task(void *arg);