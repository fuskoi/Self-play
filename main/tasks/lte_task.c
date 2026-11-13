#include "tasks_api.h"
#include "drivers.h"
#include "net_proto.h"
#include "app_config.h"
#include "power_mgr.h"
#include "esp_log.h"

static const char *TAG = "lte";

void lte_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "start");
    lte_init();

    while (1) {
        app_state_t *st = app_state_get();
        app_state_lock();
        net_mode_t mode = st->net_mode;
        sys_mode_t sys = st->sys_mode;
        app_state_unlock();

        if (sys == SYS_MODE_STANDBY || battery_is_crit()) {
            board_lte_power(false);
            mqtt_route_set_lte_ready(false);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (mode == NET_MODE_LTE || mode == NET_MODE_AUTO) {
            /* 自动模式且 Wi‑Fi 可用时可跳过；此处简化：仅 4G 模式主动上电 */
            if (mode == NET_MODE_LTE) {
                lte_power_on_sequence();
                lte_mqtt_connect("broker.emqx.io", 1883, APP_DEVICE_ID_PREFIX);
                mqtt_route_set_lte_ready(true);
            }
        }

        lte_poll();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}