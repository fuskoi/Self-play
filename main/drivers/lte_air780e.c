#include "drivers.h"
#include "board_pins.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "lte";

esp_err_t lte_init(void)
{
    uart_config_t cfg = {
        .baud_rate = LTE_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(LTE_UART_NUM, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(LTE_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(LTE_UART_NUM, PIN_LTE_TX, PIN_LTE_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "Air780E UART ready");
    return ESP_OK;
}

static void at_send(const char *cmd)
{
    uart_write_bytes(LTE_UART_NUM, cmd, strlen(cmd));
    uart_write_bytes(LTE_UART_NUM, "\r\n", 2);
}

esp_err_t lte_power_on_sequence(void)
{
    board_lte_power(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    /* PWRKEY 低脉冲开机（按模组手册微调） */
    gpio_set_level(PIN_LTE_PWRKEY, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(PIN_LTE_PWRKEY, 1);
    vTaskDelay(pdMS_TO_TICKS(3000));
    at_send("AT");
    ESP_LOGI(TAG, "power on sequence done");
    return ESP_OK;
}

esp_err_t lte_mqtt_connect(const char *host, int port, const char *client_id)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "AT+MQTTCONN=0,\"%s\",%d,1,\"%s\",\"\",\"\"", host, port, client_id);
    at_send(buf);
    /* 实际应解析模组应答成功/失败；此处骨架直接返回 */
    return ESP_OK;
}

esp_err_t lte_mqtt_publish(const char *topic, const char *payload)
{
    char buf[384];
    snprintf(buf, sizeof(buf), "AT+MQTTPUB=0,\"%s\",\"%s\",0,0", topic, payload);
    at_send(buf);
    return ESP_OK;
}

void lte_poll(void)
{
    uint8_t data[256];
    int n = uart_read_bytes(LTE_UART_NUM, data, sizeof(data) - 1, 0);
    if (n > 0) {
        data[n] = 0;
        ESP_LOGD(TAG, "rx: %s", (char *)data);
    }
}