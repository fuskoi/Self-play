#include <stdio.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "app_config.h"
#include "drivers.h"
#include "power_mgr.h"
#include "net_proto.h"
#include "tasks_api.h"

static const char *TAG = "app";

static EventGroupHandle_t s_events;
static QueueHandle_t s_count_q;

EventGroupHandle_t app_events(void) { return s_events; }
QueueHandle_t count_queue(void) { return s_count_q; }

void tasks_start_all(void)
{
    xTaskCreate(tof_task, "tof", 4096, NULL, 5, NULL);
    xTaskCreate(cam_task, "cam", 4096, NULL, 4, NULL);
    xTaskCreate(count_task, "count", 3072, NULL, 5, NULL);
    xTaskCreate(ui_task, "ui", 3072, NULL, 3, NULL);
    xTaskCreate(ble_task, "ble", 4096, NULL, 3, NULL);
    xTaskCreate(wifi_mqtt_task, "wifi", 6144, NULL, 3, NULL);
    xTaskCreate(lte_task, "lte", 4096, NULL, 3, NULL);
    xTaskCreate(power_task, "power", 3072, NULL, 4, NULL);
    xTaskCreate(telemetry_task, "telem", 3072, NULL, 3, NULL);
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
    s_events = xEventGroupCreate();
    s_count_q = xQueueCreate(16, sizeof(count_msg_t));
    mqtt_route_init();

    ESP_ERROR_CHECK(board_io_init());
    ESP_ERROR_CHECK(battery_init());
    ESP_ERROR_CHECK(tof_init());
    ESP_ERROR_CHECK(cam_init());
    ESP_ERROR_CHECK(oled_init());

    /* 恢复日累计 */
    nvs_handle_t h;
    if (nvs_open("ridership", NVS_READONLY, &h) == ESP_OK) {
        app_state_t *st = app_state_get();
        uint32_t in_c = 0, out_c = 0;
        nvs_get_u32(h, "in", &in_c);
        nvs_get_u32(h, "out", &out_c);
        nvs_close(h);
        app_state_lock();
        st->in_count = in_c;
        st->out_count = out_c;
        st->occ = (int32_t)((in_c > out_c) ? (in_c - out_c) : 0);
        app_state_unlock();
    }

    tasks_start_all();
    ESP_LOGI(TAG, "all tasks started");
}