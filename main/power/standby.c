#include "power_mgr.h"
#include "drivers.h"
#include "app_config.h"
#include "tasks_api.h"
#include "board_pins.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "standby";
static bool s_request;
static bool s_woke_tof;

void standby_request_enter(void)
{
    s_request = true;
    xEventGroupSetBits(app_events(), EVT_ENTER_STANDBY);
}

bool standby_woke_from_tof(void)
{
    return s_woke_tof;
}

void standby_service(void)
{
    if (!s_request) return;
    s_request = false;

    app_state_t *st = app_state_get();
    app_state_lock();
    st->sys_mode = SYS_MODE_STANDBY;
    app_state_unlock();

    ESP_LOGI(TAG, "enter standby");
    oled_power(false);
    board_lte_power(false);
    board_cam_power(false);
    tof_enter_autonomous_irq();

    gpio_wakeup_enable(PIN_TOF_INT, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(PIN_KEY1, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    esp_light_sleep_start();

    s_woke_tof = tof_irq_pending() || (gpio_get_level(PIN_TOF_INT) == 0);
    tof_irq_clear();
    tof_enter_active_mode();
    oled_power(true);

    app_state_lock();
    st->sys_mode = SYS_MODE_ACTIVE;
    app_state_unlock();

    xEventGroupSetBits(app_events(), EVT_EXIT_STANDBY);
    ESP_LOGI(TAG, "exit standby, tof_wake=%d", (int)s_woke_tof);
}