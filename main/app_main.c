#include <stdio.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "app_config.h"
#include "drivers.h"
#include "power_mgr.h"
#include "net_proto.h"
#include "tasks_api.h"
#include "tof_api.h"

static const char *TAG = "app";

static EventGroupHandle_t s_events;
static QueueHandle_t s_count_q;

EventGroupHandle_t app_events(void) { return s_events; }
QueueHandle_t count_queue(void) { return s_count_q; }

void tasks_start_all(void)
{
    xTaskCreate(tof_task, "tof", 5120, NULL, 5, NULL);
    xTaskCreate(cam_task, "cam", 4096, NULL, 4, NULL);
    xTaskCreate(count_task, "count", 3072, NULL, 5, NULL);
    xTaskCreate(ui_task, "ui", 3072, NULL, 3, NULL);
    xTaskCreate(ble_task, "ble", 6144, NULL, 3, NULL);
    xTaskCreate(wifi_mqtt_task, "wifi", 8192, NULL, 3, NULL);
    xTaskCreate(lte_task, "lte", 4096, NULL, 3, NULL);
    xTaskCreate(power_task, "power", 3072, NULL, 4, NULL);
    xTaskCreate(telemetry_task, "telem", 3072, NULL, 3, NULL);
    xTaskCreate(console_task, "console", 3072, NULL, 2, NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "ridership boot");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    app_state_init();
    app_state_load_nvs();
    s_events = xEventGroupCreate();
    s_count_q = xQueueCreate(16, sizeof(count_msg_t));
    mqtt_route_init();

    ESP_ERROR_CHECK(board_io_init());
    ESP_ERROR_CHECK(battery_init());
    ESP_ERROR_CHECK(tof_init());
    ESP_ERROR_CHECK(app_cam_init());
    ESP_ERROR_CHECK(oled_init());

    tasks_start_all();

    /* 上电空场背景标定（无传感器时仿真帧也可跑通流程） */
    tof_start_bg_calibrate();

    ESP_LOGI(TAG, "all tasks started");
}
