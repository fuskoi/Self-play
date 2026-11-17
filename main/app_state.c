#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "state";
static app_state_t s_st;
static SemaphoreHandle_t s_mtx;

void app_state_init(void)
{
    memset(&s_st, 0, sizeof(s_st));
    s_st.net_mode = NET_MODE_AUTO;
    s_st.sys_mode = SYS_MODE_ACTIVE;
    s_st.occ_threshold = APP_OCC_THRESHOLD_DEFAULT;
    s_st.idle_ms = APP_T_IDLE_MS;
    s_st.bat_percent = 100;
    s_st.install_h_m = APP_INSTALL_H_M_DEFAULT;
    s_st.gate_y = APP_GATE_Y_DEFAULT;
    s_st.bg_calibrated = false;
    s_mtx = xSemaphoreCreateMutex();
}

app_state_t *app_state_get(void)
{
    return &s_st;
}

void app_state_lock(void)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
}

void app_state_unlock(void)
{
    xSemaphoreGive(s_mtx);
}

void app_state_persist_counts(void)
{
    nvs_handle_t h;
    if (nvs_open("ridership", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_set_u32(h, "in", s_st.in_count);
    nvs_set_u32(h, "out", s_st.out_count);
    nvs_commit(h);
    nvs_close(h);
}

void app_state_persist_params(void)
{
    nvs_handle_t h;
    if (nvs_open("ridership", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_set_u16(h, "thresh", s_st.occ_threshold);
    nvs_set_u32(h, "idle_ms", s_st.idle_ms);
    nvs_set_u8(h, "net_mode", (uint8_t)s_st.net_mode);
    uint32_t h_mm = (uint32_t)(s_st.install_h_m * 1000.0f + 0.5f);
    uint32_t gate_x100 = (uint32_t)(s_st.gate_y * 100.0f + 0.5f);
    nvs_set_u32(h, "install_mm", h_mm);
    nvs_set_u32(h, "gate_x100", gate_x100);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "params saved H=%.2f gate=%.2f thr=%u",
             s_st.install_h_m, s_st.gate_y, (unsigned)s_st.occ_threshold);
}

void app_state_load_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open("ridership", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    uint32_t in_c = 0, out_c = 0, idle = 0, hmm = 0, gx = 0;
    uint16_t thr = 0;
    uint8_t nm = 0;
    if (nvs_get_u32(h, "in", &in_c) == ESP_OK) {
        s_st.in_count = in_c;
    }
    if (nvs_get_u32(h, "out", &out_c) == ESP_OK) {
        s_st.out_count = out_c;
    }
    int64_t occ = (int64_t)s_st.in_count - (int64_t)s_st.out_count;
    s_st.occ = (occ < 0) ? 0 : (int32_t)occ;

    if (nvs_get_u16(h, "thresh", &thr) == ESP_OK && thr > 0) {
        s_st.occ_threshold = thr;
    }
    if (nvs_get_u32(h, "idle_ms", &idle) == ESP_OK && idle >= 3000) {
        s_st.idle_ms = idle;
    }
    if (nvs_get_u8(h, "net_mode", &nm) == ESP_OK && nm <= 2) {
        s_st.net_mode = (net_mode_t)nm;
    }
    if (nvs_get_u32(h, "install_mm", &hmm) == ESP_OK && hmm >= 1500 && hmm <= 3500) {
        s_st.install_h_m = hmm / 1000.0f;
    }
    if (nvs_get_u32(h, "gate_x100", &gx) == ESP_OK && gx >= 100 && gx <= 700) {
        s_st.gate_y = gx / 100.0f;
    }
    nvs_close(h);
    ESP_LOGI(TAG, "nvs in=%lu out=%lu occ=%ld H=%.2f",
             (unsigned long)s_st.in_count, (unsigned long)s_st.out_count,
             (long)s_st.occ, s_st.install_h_m);
}
