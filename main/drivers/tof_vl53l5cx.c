#include "drivers.h"
#include "board_pins.h"
#include "i2c_bus.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "vl53l5cx_api.h"
#include "vl53l5cx_plugin_detection_thresholds.h"
#include <string.h>

static const char *TAG = "tof";

/* 8 位 I2C 地址（ST ULD 约定）；A 改址后 B 保持默认，避免总线冲突 */
#define TOF_ADDR_DEFAULT_8BIT  VL53L5CX_DEFAULT_I2C_ADDRESS /* 默认 0x52 */
#define TOF_ADDR_A_8BIT        ((uint16_t)0x54)
#define TOF_ADDR_B_8BIT        TOF_ADDR_DEFAULT_8BIT

#define TOF_HZ_ACTIVE          15
#define TOF_HZ_STANDBY         2
#define TOF_INVALID_MM         4000
#define TOF_STATUS_OK_A        5
#define TOF_STATUS_OK_B        9

static bool s_irq;
static bool s_sim;          /* true=仿真；默认先尝试硬件，失败再回退 */
static bool s_sim_force;    /* 串口命令强制仿真，跳过硬件 */
static bool s_hw_ok;        /* A 或 B 至少一颗可用 */
static bool s_hw_a;
static bool s_hw_b;
static bool s_ranging;

static VL53L5CX_Configuration s_dev_a;
static VL53L5CX_Configuration s_dev_b;
static VL53L5CX_ResultsData s_res_a;
static VL53L5CX_ResultsData s_res_b;

static void fill_const(uint16_t d[APP_TOF_ZONE_N], uint16_t mm)
{
    for (int i = 0; i < APP_TOF_ZONE_N; i++) {
        d[i] = mm;
    }
}

static void sim_fill(uint16_t a[APP_TOF_ZONE_N], uint16_t b[APP_TOF_ZONE_N])
{
    fill_const(a, 2200);
    fill_const(b, 2200);
    int64_t us = esp_timer_get_time();
    int phase = (int)((us / 200000) % 20);
    int y = (phase < 10) ? phase : (19 - phase);
    int x = 3;
    int idx = y * 8 + x;
    if (idx >= 0 && idx < APP_TOF_ZONE_N) {
        a[idx] = 1200;
        if (idx + 1 < APP_TOF_ZONE_N) {
            a[idx + 1] = 1200;
        }
        b[idx] = 1250;
    }
}

static void xshut_set(gpio_num_t pin, int level)
{
    gpio_set_level(pin, level);
}

static esp_err_t i2c_bus_init(void)
{
    return i2c_bus_ensure_init();
}

static bool status_valid(uint8_t st)
{
    return (st == TOF_STATUS_OK_A) || (st == TOF_STATUS_OK_B);
}

static void results_to_zones(const VL53L5CX_ResultsData *res, uint16_t out[APP_TOF_ZONE_N])
{
    for (int i = 0; i < APP_TOF_ZONE_N; i++) {
        uint8_t n = 0;
#ifndef VL53L5CX_DISABLE_NB_TARGET_DETECTED
        n = res->nb_target_detected[i];
#else
        n = 1;
#endif
        uint8_t st = 0;
#ifndef VL53L5CX_DISABLE_TARGET_STATUS
        st = res->target_status[i];
#endif
        int16_t dist = TOF_INVALID_MM;
#ifndef VL53L5CX_DISABLE_DISTANCE_MM
        dist = res->distance_mm[i];
#endif
        if (n > 0 && status_valid(st) && dist > 0) {
            out[i] = (uint16_t)dist;
        } else {
            out[i] = TOF_INVALID_MM;
        }
    }
}

static uint8_t configure_sensor(VL53L5CX_Configuration *dev, const char *name)
{
    uint8_t st;
    uint8_t is_alive = 0;

    st = vl53l5cx_is_alive(dev, &is_alive);
    if (st != VL53L5CX_STATUS_OK || !is_alive) {
        ESP_LOGE(TAG, "%s not alive (st=%u alive=%u)", name, st, is_alive);
        return VL53L5CX_STATUS_ERROR;
    }

    st = vl53l5cx_init(dev);
    if (st != VL53L5CX_STATUS_OK) {
        ESP_LOGE(TAG, "%s init failed (%u)", name, st);
        return st;
    }

    st |= vl53l5cx_set_resolution(dev, VL53L5CX_RESOLUTION_8X8);
    st |= vl53l5cx_set_ranging_frequency_hz(dev, TOF_HZ_ACTIVE);
    st |= vl53l5cx_set_target_order(dev, VL53L5CX_TARGET_ORDER_CLOSEST);
    st |= vl53l5cx_set_ranging_mode(dev, VL53L5CX_RANGING_MODE_CONTINUOUS);
    if (st != VL53L5CX_STATUS_OK) {
        ESP_LOGE(TAG, "%s config failed (%u)", name, st);
        return st;
    }

    ESP_LOGI(TAG, "%s ready @0x%02X", name, (unsigned)dev->platform.address);
    return VL53L5CX_STATUS_OK;
}

static esp_err_t hw_init_dual(void)
{
    /* 双传感器：先只拉起 A，改址后再拉起 B（默认地址）。
     * 单颗联调：仅 A 或仅 B 成功也算硬件可用，不再整组退仿真。 */
    s_hw_a = false;
    s_hw_b = false;

    xshut_set(PIN_TOF_A_XSHUT, 0);
    xshut_set(PIN_TOF_B_XSHUT, 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    xshut_set(PIN_TOF_A_XSHUT, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    memset(&s_dev_a, 0, sizeof(s_dev_a));
    vl53l5cx_esp_platform_init(&s_dev_a.platform, TOF_ADDR_DEFAULT_8BIT);
    if (vl53l5cx_set_i2c_address(&s_dev_a, TOF_ADDR_A_8BIT) != VL53L5CX_STATUS_OK) {
        ESP_LOGW(TAG, "A set address failed (no sensor on XSHUT=A?)");
    } else if (configure_sensor(&s_dev_a, "ToF-A") == VL53L5CX_STATUS_OK) {
        s_hw_a = true;
    }

    xshut_set(PIN_TOF_B_XSHUT, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    memset(&s_dev_b, 0, sizeof(s_dev_b));
    vl53l5cx_esp_platform_init(&s_dev_b.platform, TOF_ADDR_B_8BIT);
    if (configure_sensor(&s_dev_b, "ToF-B") == VL53L5CX_STATUS_OK) {
        s_hw_b = true;
    } else {
        ESP_LOGW(TAG, "ToF-B not present — single-sensor OK for bring-up");
        xshut_set(PIN_TOF_B_XSHUT, 0);
    }

    if (!s_hw_a && !s_hw_b) {
        return ESP_FAIL;
    }

    uint8_t st = 0;
    if (s_hw_a) {
        st |= vl53l5cx_start_ranging(&s_dev_a);
    }
    if (s_hw_b) {
        st |= vl53l5cx_start_ranging(&s_dev_b);
    }
    if (st != VL53L5CX_STATUS_OK) {
        ESP_LOGE(TAG, "start_ranging failed (%u)", st);
        return ESP_FAIL;
    }
    s_ranging = true;
    ESP_LOGI(TAG, "tof HW ready A=%d B=%d", (int)s_hw_a, (int)s_hw_b);
    return ESP_OK;
}

esp_err_t tof_init(void)
{
    esp_err_t err = i2c_bus_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c init fail %s", esp_err_to_name(err));
        s_sim = true;
        s_hw_ok = false;
        return err;
    }

    gpio_config_t io = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_TOF_A_XSHUT) | (1ULL << PIN_TOF_B_XSHUT),
    };
    gpio_config(&io);
    gpio_config_t in = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_TOF_INT),
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&in);

    if (s_sim_force) {
        ESP_LOGW(TAG, "tof init (SIM forced)");
        s_sim = true;
        s_hw_ok = false;
        return ESP_OK;
    }

    err = hw_init_dual();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HW init failed, fallback SIM");
        s_sim = true;
        s_hw_ok = false;
        return ESP_OK; /* 允许无传感器继续跑算法 */
    }

    s_hw_ok = true;
    s_sim = false;
    ESP_LOGI(TAG, "tof init (HW VL53L5CX A=%d B=%d)", (int)s_hw_a, (int)s_hw_b);
    return ESP_OK;
}

esp_err_t tof_read_both(uint16_t a[APP_TOF_ZONE_N], uint16_t b[APP_TOF_ZONE_N])
{
    if (!a || !b) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_sim || !s_hw_ok) {
        sim_fill(a, b);
        return ESP_OK;
    }

    uint8_t ready = 0;
    uint8_t st;

    if (s_hw_a) {
        st = vl53l5cx_check_data_ready(&s_dev_a, &ready);
        if (st == VL53L5CX_STATUS_OK && ready) {
            if (vl53l5cx_get_ranging_data(&s_dev_a, &s_res_a) == VL53L5CX_STATUS_OK) {
                results_to_zones(&s_res_a, a);
            } else {
                fill_const(a, TOF_INVALID_MM);
            }
        } else {
            fill_const(a, TOF_INVALID_MM);
        }
    } else {
        fill_const(a, TOF_INVALID_MM);
    }

    if (s_hw_b) {
        ready = 0;
        st = vl53l5cx_check_data_ready(&s_dev_b, &ready);
        if (st == VL53L5CX_STATUS_OK && ready) {
            if (vl53l5cx_get_ranging_data(&s_dev_b, &s_res_b) == VL53L5CX_STATUS_OK) {
                results_to_zones(&s_res_b, b);
            } else {
                fill_const(b, TOF_INVALID_MM);
            }
        } else {
            fill_const(b, TOF_INVALID_MM);
        }
    } else {
        fill_const(b, TOF_INVALID_MM);
    }

    return ESP_OK;
}

esp_err_t tof_enter_autonomous_irq(void)
{
    ESP_LOGI(TAG, "autonomous irq mode");
    if (s_sim || !s_hw_ok) {
        return ESP_OK;
    }

    if (s_ranging) {
        if (s_hw_a) {
            vl53l5cx_stop_ranging(&s_dev_a);
        }
        if (s_hw_b) {
            vl53l5cx_stop_ranging(&s_dev_b);
        }
        s_ranging = false;
    }

    /* 低帧率 + 距离阈值中断：任一区出现较近目标则拉低中断脚 */
    VL53L5CX_DetectionThresholds thresholds[VL53L5CX_NB_THRESHOLDS];
    memset(thresholds, 0, sizeof(thresholds));
    for (int i = 0; i < (VL53L5CX_NB_THRESHOLDS - 1); i++) {
        thresholds[i].zone_num = (uint8_t)i;
        thresholds[i].measurement = VL53L5CX_DISTANCE_MM;
        thresholds[i].type = VL53L5CX_LESS_THAN_EQUAL_MIN_CHECKER;
        thresholds[i].mathematic_operation = VL53L5CX_OPERATION_OR;
        thresholds[i].param_low_thresh = 1800; /* mm：有人近于约 1.8m */
        thresholds[i].param_high_thresh = 0;
    }
    thresholds[VL53L5CX_NB_THRESHOLDS - 1].zone_num = VL53L5CX_LAST_THRESHOLD;

    if (s_hw_a) {
        vl53l5cx_set_detection_thresholds_enable(&s_dev_a, 0);
        vl53l5cx_set_detection_thresholds(&s_dev_a, thresholds);
        vl53l5cx_set_detection_thresholds_enable(&s_dev_a, 1);
        vl53l5cx_set_ranging_mode(&s_dev_a, VL53L5CX_RANGING_MODE_AUTONOMOUS);
        vl53l5cx_set_ranging_frequency_hz(&s_dev_a, TOF_HZ_STANDBY);
        vl53l5cx_start_ranging(&s_dev_a);
    }
    if (s_hw_b) {
        vl53l5cx_set_power_mode(&s_dev_b, VL53L5CX_POWER_MODE_SLEEP);
    }

    s_ranging = true;
    return ESP_OK;
}

esp_err_t tof_enter_active_mode(void)
{
    ESP_LOGI(TAG, "active ranging mode");
    if (s_sim || !s_hw_ok) {
        return ESP_OK;
    }

    if (s_ranging) {
        if (s_hw_a) {
            vl53l5cx_stop_ranging(&s_dev_a);
        }
        if (s_hw_b) {
            vl53l5cx_stop_ranging(&s_dev_b);
        }
        s_ranging = false;
    }

    if (s_hw_a) {
        vl53l5cx_set_detection_thresholds_enable(&s_dev_a, 0);
        vl53l5cx_set_ranging_mode(&s_dev_a, VL53L5CX_RANGING_MODE_CONTINUOUS);
        vl53l5cx_set_ranging_frequency_hz(&s_dev_a, TOF_HZ_ACTIVE);
        vl53l5cx_set_resolution(&s_dev_a, VL53L5CX_RESOLUTION_8X8);
        vl53l5cx_start_ranging(&s_dev_a);
    }
    if (s_hw_b) {
        vl53l5cx_set_power_mode(&s_dev_b, VL53L5CX_POWER_MODE_WAKEUP);
        vl53l5cx_set_ranging_mode(&s_dev_b, VL53L5CX_RANGING_MODE_CONTINUOUS);
        vl53l5cx_set_ranging_frequency_hz(&s_dev_b, TOF_HZ_ACTIVE);
        vl53l5cx_set_resolution(&s_dev_b, VL53L5CX_RESOLUTION_8X8);
        vl53l5cx_start_ranging(&s_dev_b);
    }
    s_ranging = true;
    return ESP_OK;
}

bool tof_irq_pending(void)
{
    if (gpio_get_level(PIN_TOF_INT) == 0) {
        s_irq = true;
    }
    return s_irq;
}

void tof_irq_clear(void)
{
    s_irq = false;
}

void tof_set_sim(bool enable)
{
    s_sim_force = enable;
    s_sim = enable || !s_hw_ok;
    ESP_LOGI(TAG, "sim %s (hw_ok=%d)", enable ? "on" : "off", (int)s_hw_ok);
}

bool tof_sim_enabled(void)
{
    return s_sim;
}
