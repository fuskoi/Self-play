#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "app_config.h"

esp_err_t board_io_init(void);
bool      board_key1_pressed(void);
bool      board_key2_pressed(void);
void      board_buzzer_beep(int pattern);
void      board_lte_power(bool on);
void      board_cam_power(bool on);

esp_err_t tof_init(void);
esp_err_t tof_read_both(uint16_t a[APP_TOF_ZONE_N], uint16_t b[APP_TOF_ZONE_N]);
esp_err_t tof_enter_autonomous_irq(void);
esp_err_t tof_enter_active_mode(void);
bool      tof_irq_pending(void);
void      tof_irq_clear(void);

esp_err_t cam_init(void);
esp_err_t cam_capture_gray(uint8_t **out_buf, int *w, int *h);
void      cam_release(uint8_t *buf);

esp_err_t oled_init(void);
void      oled_clear(void);
void      oled_show_main(uint32_t in_c, uint32_t out_c, int32_t occ);
void      oled_show_status(uint8_t bat, const char *net, uint8_t alarm);
void      oled_power(bool on);

esp_err_t lte_init(void);
esp_err_t lte_power_on_sequence(void);
esp_err_t lte_mqtt_connect(const char *host, int port, const char *client_id);
esp_err_t lte_mqtt_publish(const char *topic, const char *payload);
void      lte_poll(void);