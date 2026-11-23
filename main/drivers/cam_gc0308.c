#include "drivers.h"
#include "board_pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_camera.h"
#include <stdlib.h>
#include <string.h>

/*
 * GC0308（GalaxyCore VGA）触发式采集
 * 手册摘要：
 *   - 供电：CAM_CTRL 切 2V8/1V5 LDO；IOVDD=2.8 V
 *   - SCCB 写 0x42 / 读 0x43；Chip_ID=0x9b
 *   - 8bit DVP 用 Y2..Y9；人数辅助灰度 QQVGA
 *   - 平时断电；多人嫌疑时上电→采帧→断电
 */

static const char *TAG = "cam";

#define CAM_W_QQVGA  160
#define CAM_H_QQVGA  120

static bool s_driver_up;
static bool s_hw_ok; /* 曾成功 init；失败则回退合成图便于无硬件联调算法 */

static camera_config_t make_cfg(void)
{
    camera_config_t c = {
        .pin_pwdn = PIN_CAM_PWDN,
        .pin_reset = PIN_CAM_RESET,
        .pin_xclk = PIN_CAM_XCLK,
        .pin_sccb_sda = PIN_CAM_SCCB_SDA,
        .pin_sccb_scl = PIN_CAM_SCCB_SCL,
        .pin_d7 = PIN_CAM_D7,
        .pin_d6 = PIN_CAM_D6,
        .pin_d5 = PIN_CAM_D5,
        .pin_d4 = PIN_CAM_D4,
        .pin_d3 = PIN_CAM_D3,
        .pin_d2 = PIN_CAM_D2,
        .pin_d1 = PIN_CAM_D1,
        .pin_d0 = PIN_CAM_D0,
        .pin_vsync = PIN_CAM_VSYNC,
        .pin_href = PIN_CAM_HREF,
        .pin_pclk = PIN_CAM_PCLK,
        .xclk_freq_hz = 10000000, /* 10 MHz，兼顾稳定与功耗 */
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_GRAYSCALE,
        .frame_size = FRAMESIZE_QQVGA,
        .jpeg_quality = 12,
        .fb_count = 1,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
        .sccb_i2c_port = 1, /* 独立于 ToF/OLED 的 I2C0 */
    };
    return c;
}

static esp_err_t cam_hw_start(void)
{
    if (s_driver_up) {
        return ESP_OK;
    }
    /* 开发板飞线阶段未接 GC0308 时脚为 -1，直接 stub */
    if (PIN_CAM_XCLK < 0 || PIN_CAM_PCLK < 0) {
        s_hw_ok = false;
        return ESP_ERR_NOT_SUPPORTED;
    }
    board_cam_power(true);
    vTaskDelay(pdMS_TO_TICKS(80)); /* 供电/复位稳定 */

    camera_config_t cfg = make_cfg();
    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_camera_init: %s — use stub frames", esp_err_to_name(err));
        board_cam_power(false);
        s_hw_ok = false;
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        ESP_LOGI(TAG, "sensor PID=0x%02x (expect GC0308 0x9b)", s->id.PID);
        /* 俯视安装：按模组方向可调 */
        s->set_vflip(s, 1);
        s->set_hmirror(s, 0);
        if (s->set_framesize) {
            s->set_framesize(s, FRAMESIZE_QQVGA);
        }
        if (s->set_pixformat) {
            s->set_pixformat(s, PIXFORMAT_GRAYSCALE);
        }
    }
    s_driver_up = true;
    s_hw_ok = true;
    return ESP_OK;
}

static void cam_hw_stop(void)
{
    if (s_driver_up) {
        esp_camera_deinit();
        s_driver_up = false;
    }
    board_cam_power(false);
}

static esp_err_t stub_gray(uint8_t **out_buf, int *w, int *h)
{
    const int W = CAM_W_QQVGA, H = CAM_H_QQVGA;
    uint8_t *buf = (uint8_t *)malloc((size_t)W * H);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    memset(buf, 180, (size_t)W * H);
    for (int y = 40; y < 70; y++) {
        for (int x = 40; x < 70; x++) {
            buf[y * W + x] = 40;
        }
        for (int x = 95; x < 125; x++) {
            buf[y * W + x] = 40;
        }
    }
    *out_buf = buf;
    *w = W;
    *h = H;
    ESP_LOGW(TAG, "stub gray %dx%d (no sensor)", W, H);
    return ESP_OK;
}

esp_err_t app_cam_init(void)
{
    /* 启动时不常开相机；仅探测日志 */
    ESP_LOGI(TAG, "GC0308 ready (trigger mode, PWR_EN=%d)", PIN_CAM_PWR_EN);
    board_cam_power(false);
    s_driver_up = false;
    s_hw_ok = false;
    return ESP_OK;
}

esp_err_t app_cam_capture_gray(uint8_t **out_buf, int *w, int *h)
{
    if (!out_buf || !w || !h) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_buf = NULL;
    *w = *h = 0;

    if (cam_hw_start() != ESP_OK) {
        return stub_gray(out_buf, w, h);
    }

    /* 丢弃 1～2 帧，等 AE 收敛 */
    for (int i = 0; i < 2; i++) {
        camera_fb_t *drop = esp_camera_fb_get();
        if (drop) {
            esp_camera_fb_return(drop);
        }
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb || !fb->buf || fb->len == 0) {
        ESP_LOGE(TAG, "fb_get failed");
        cam_hw_stop();
        return stub_gray(out_buf, w, h);
    }

    int width = fb->width > 0 ? (int)fb->width : CAM_W_QQVGA;
    int height = fb->height > 0 ? (int)fb->height : CAM_H_QQVGA;
    size_t need = (size_t)width * (size_t)height;

    uint8_t *copy = (uint8_t *)malloc(need);
    if (!copy) {
        esp_camera_fb_return(fb);
        cam_hw_stop();
        return ESP_ERR_NO_MEM;
    }

    if (fb->format == PIXFORMAT_GRAYSCALE && fb->len >= need) {
        memcpy(copy, fb->buf, need);
    } else if (fb->len >= need * 2) {
        /* YUV422：取 Y */
        for (size_t i = 0; i < need; i++) {
            copy[i] = fb->buf[i * 2];
        }
    } else {
        size_t n = fb->len < need ? fb->len : need;
        memcpy(copy, fb->buf, n);
        if (n < need) {
            memset(copy + n, 128, need - n);
        }
    }

    esp_camera_fb_return(fb);
    cam_hw_stop(); /* 触发式：采完即断电 */

    *out_buf = copy;
    *w = width;
    *h = height;
    ESP_LOGI(TAG, "capture gray %dx%d ok", width, height);
    return ESP_OK;
}

void app_cam_release(uint8_t *buf)
{
    free(buf);
    if (s_driver_up) {
        cam_hw_stop();
    } else {
        board_cam_power(false);
    }
}
