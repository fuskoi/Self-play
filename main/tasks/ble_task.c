#include "tasks_api.h"
#include "net_proto.h"
#include "esp_log.h"

static const char *TAG = "ble";

/* 低功耗蓝牙 GATT 完整实现可后续按 UUID 表补齐；此处保持可链接骨架 */

void ble_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "BLE task stub — enable NimBLE GATT next");
    mqtt_route_set_ble_connected(false);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}