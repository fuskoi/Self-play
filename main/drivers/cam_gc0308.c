#include "drivers.h"
#include "board_pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "cam";

/* 待办：接入官方相机组件或自研并行接口驱动 GC0308
 * 当前返回合成灰度图，便于算法联调。
 */

esp_err_t cam_init(void)
{
    ESP_LOGI(TAG, "GC0308 stub init");
    return ESP_OK;
}

esp_err_t cam_capture_gray(uint8_t **out_buf, int *w, int *h)
{
    board_cam_power(true);
    vTaskDelay(pdMS_TO_TICKS(50));

    const int W = 160, H = 120;
    uint8_t *buf = (uint8_t *)malloc(W * H);
    if (!buf) {
        board_cam_power(false);
        return ESP_ERR_NO_MEM;
    }
    memset(buf, 180, W * H);
    /* 画两个暗斑模拟双人 */
    for (int y = 40; y < 70; y++) {
        for (int x = 40; x < 70; x++) buf[y * W + x] = 40;
        for (int x = 95; x < 125; x++) buf[y * W + x] = 40;
    }

    *out_buf = buf;
    *w = W;
    *h = H;
    ESP_LOGI(TAG, "capture gray %dx%d (stub)", W, H);
    return ESP_OK;
}

void cam_release(uint8_t *buf)
{
    free(buf);
    board_cam_power(false);
}