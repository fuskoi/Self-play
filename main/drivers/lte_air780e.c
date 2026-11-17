#include "drivers.h"
#include "board_pins.h"
#include "app_config.h"
#include "net_proto.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * 合宙 Air780E（银尔达 M100M AT 固件等同系）UART AT 驱动
 *
 * MQTT（合宙 AT）：
 *   AT+MCONFIG=<clientid>,<user>,<pass>
 *   AT+MIPSTART="<host>","<port>"   → CONNECT OK
 *   AT+MCONNECT=1,<keepalive>       → CONNACK OK
 *   AT+MPUB="<topic>",<qos>,<retain>,"<payload>"
 *   AT+MDISCONNECT / AT+MIPCLOSE
 *
 * 注：Quectel 风格 AT+MQTTCONN / AT+MQTTPUB 不适用于本模组。
 */

static const char *TAG = "lte";

#define LTE_RX_BUF_SZ     2048
#define LTE_LINE_MAX      512
#define LTE_AT_RETRIES    8

static bool s_uart_ok;
static bool s_powered;
static bool s_mqtt_up;
static char s_rx_acc[LTE_LINE_MAX];
static int  s_rx_len;

static void lte_uart_flush(void)
{
    uart_flush_input(LTE_UART_NUM);
    s_rx_len = 0;
    s_rx_acc[0] = '\0';
}

static void at_write_raw(const char *s)
{
    if (!s) {
        return;
    }
    uart_write_bytes(LTE_UART_NUM, s, strlen(s));
}

static void at_send_line(const char *cmd)
{
    ESP_LOGI(TAG, ">> %s", cmd);
    at_write_raw(cmd);
    at_write_raw("\r\n");
}

/* 在 timeout_ms 内累积串口数据，命中 expect 子串则成功；expect=NULL 只收数据 */
static esp_err_t at_wait_contains(const char *expect, int timeout_ms)
{
    int64_t deadline = (int64_t)xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while ((int64_t)xTaskGetTickCount() < deadline) {
        uint8_t tmp[128];
        int n = uart_read_bytes(LTE_UART_NUM, tmp, sizeof(tmp), pdMS_TO_TICKS(50));
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                if (s_rx_len < LTE_LINE_MAX - 1) {
                    s_rx_acc[s_rx_len++] = (char)tmp[i];
                    s_rx_acc[s_rx_len] = '\0';
                } else {
                    /* 滑动窗口，保留后半段 */
                    memmove(s_rx_acc, s_rx_acc + LTE_LINE_MAX / 2, LTE_LINE_MAX / 2);
                    s_rx_len = LTE_LINE_MAX / 2;
                    s_rx_acc[s_rx_len++] = (char)tmp[i];
                    s_rx_acc[s_rx_len] = '\0';
                }
            }
            if (expect && strstr(s_rx_acc, expect)) {
                ESP_LOGD(TAG, "<< hit [%s]", expect);
                return ESP_OK;
            }
            if (expect && strstr(s_rx_acc, "ERROR")) {
                ESP_LOGW(TAG, "<< ERROR while waiting [%s]: %s", expect, s_rx_acc);
                return ESP_FAIL;
            }
        }
    }
    if (expect) {
        ESP_LOGW(TAG, "timeout wait [%s], buf=%s", expect, s_rx_acc);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static esp_err_t at_cmd(const char *cmd, const char *expect, int timeout_ms)
{
    lte_uart_flush();
    s_rx_len = 0;
    s_rx_acc[0] = '\0';
    at_send_line(cmd);
    if (!expect) {
        vTaskDelay(pdMS_TO_TICKS(timeout_ms > 0 ? timeout_ms : 100));
        return ESP_OK;
    }
    return at_wait_contains(expect, timeout_ms);
}

static esp_err_t at_cmd_ok(const char *cmd, int timeout_ms)
{
    return at_cmd(cmd, "OK", timeout_ms);
}

esp_err_t lte_init(void)
{
    if (s_uart_ok) {
        return ESP_OK;
    }
    uart_config_t cfg = {
        .baud_rate = LTE_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(LTE_UART_NUM, LTE_RX_BUF_SZ, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(LTE_UART_NUM, &cfg));
    /* ESP TX -> 模组 RXD，ESP RX <- 模组 TXD */
    ESP_ERROR_CHECK(uart_set_pin(LTE_UART_NUM, PIN_LTE_TX, PIN_LTE_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    s_uart_ok = true;
    s_powered = false;
    s_mqtt_up = false;
    ESP_LOGI(TAG, "Air780E UART ready @%d", LTE_UART_BAUD);
    return ESP_OK;
}

esp_err_t lte_power_off(void)
{
    if (s_mqtt_up) {
        at_cmd("AT+MDISCONNECT", "OK", 3000);
        at_cmd("AT+MIPCLOSE", "OK", 3000);
        s_mqtt_up = false;
    }
    /* 尝试软关机；失败则直接断电 */
    at_cmd("AT+CPOWD=1", "NORMAL POWER DOWN", 5000);
    gpio_set_level(PIN_LTE_PWRKEY, 1);
    board_lte_power(false);
    s_powered = false;
    ESP_LOGI(TAG, "powered off");
    return ESP_OK;
}

esp_err_t lte_power_on_sequence(void)
{
    if (!s_uart_ok) {
        if (lte_init() != ESP_OK) {
            return ESP_FAIL;
        }
    }
    if (s_powered) {
        /* 已上电则探活 */
        if (at_cmd_ok("AT", 1000) == ESP_OK) {
            return ESP_OK;
        }
        s_powered = false;
        s_mqtt_up = false;
    }

    board_lte_power(true);
    vTaskDelay(pdMS_TO_TICKS(200));

    /*
     * PWRKEY：合宙模组需拉低约 1s 开机。
     * 银尔达 M100M 部分板子上电自启，脉冲仍无害；若改接 RST，
     * 可把 PIN_LTE_PWRKEY 接到 RST 并改为短低脉冲复位。
     */
    gpio_set_level(PIN_LTE_PWRKEY, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_LTE_PWRKEY, 0);
    vTaskDelay(pdMS_TO_TICKS(1200));
    gpio_set_level(PIN_LTE_PWRKEY, 1);

    /* 等待模组启动 */
    vTaskDelay(pdMS_TO_TICKS(2500));
    lte_uart_flush();

    esp_err_t ok = ESP_FAIL;
    for (int i = 0; i < LTE_AT_RETRIES; i++) {
        if (at_cmd_ok("AT", 1000) == ESP_OK) {
            ok = ESP_OK;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (ok != ESP_OK) {
        ESP_LOGE(TAG, "AT not ready");
        board_lte_power(false);
        return ESP_FAIL;
    }

    at_cmd_ok("ATE0", 1000);          /* 关回显 */
    at_cmd_ok("AT+CMEE=2", 1000);     /* 详细错误 */
    s_powered = true;
    ESP_LOGI(TAG, "power on OK");
    return ESP_OK;
}

esp_err_t lte_wait_network(int timeout_ms)
{
    if (!s_powered && lte_power_on_sequence() != ESP_OK) {
        return ESP_FAIL;
    }

    /* SIM */
    if (at_cmd("AT+CPIN?", "READY", 5000) != ESP_OK) {
        ESP_LOGE(TAG, "SIM not READY");
        return ESP_FAIL;
    }

    int64_t deadline = (int64_t)xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while ((int64_t)xTaskGetTickCount() < deadline) {
        /* 信号：AT+CSQ → +CSQ: <rssi>,<ber>  rssi 0~31 或 99 */
        lte_uart_flush();
        s_rx_len = 0;
        s_rx_acc[0] = '\0';
        at_send_line("AT+CSQ");
        at_wait_contains("OK", 2000);
        ESP_LOGI(TAG, "CSQ: %s", s_rx_acc);

        /* EPS 注册：+CEREG: n,1 或 n,5 */
        lte_uart_flush();
        s_rx_len = 0;
        s_rx_acc[0] = '\0';
        at_send_line("AT+CEREG?");
        if (at_wait_contains("OK", 2000) == ESP_OK) {
            if (strstr(s_rx_acc, ",1") || strstr(s_rx_acc, ",5")) {
                if (at_cmd("AT+CGATT?", "+CGATT: 1", 3000) == ESP_OK ||
                    at_cmd_ok("AT+CGATT=1", 10000) == ESP_OK) {
                    ESP_LOGI(TAG, "network attached");
                    return ESP_OK;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    ESP_LOGE(TAG, "network attach timeout");
    return ESP_ERR_TIMEOUT;
}

static esp_err_t mqtt_close_quiet(void)
{
    at_cmd("AT+MDISCONNECT", NULL, 500);
    at_cmd("AT+MIPCLOSE", NULL, 500);
    s_mqtt_up = false;
    return ESP_OK;
}

esp_err_t lte_mqtt_connect(const char *host, int port, const char *client_id)
{
    if (!host || !client_id) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_powered && lte_power_on_sequence() != ESP_OK) {
        return ESP_FAIL;
    }
    if (lte_wait_network(APP_LTE_NET_TIMEOUT_MS) != ESP_OK) {
        return ESP_FAIL;
    }

    mqtt_close_quiet();
    vTaskDelay(pdMS_TO_TICKS(200));

    char buf[192];
    /* clientid,username,password — 公开 broker 可空用户密码 */
    snprintf(buf, sizeof(buf), "AT+MCONFIG=%s,%s,%s",
             client_id, APP_LTE_MQTT_USER, APP_LTE_MQTT_PASS);
    if (at_cmd_ok(buf, 3000) != ESP_OK) {
        return ESP_FAIL;
    }

    snprintf(buf, sizeof(buf), "AT+MIPSTART=\"%s\",\"%d\"", host, port);
    lte_uart_flush();
    s_rx_len = 0;
    s_rx_acc[0] = '\0';
    at_send_line(buf);
    /* 合宙：先 OK，再异步 CONNECT OK */
    if (at_wait_contains("CONNECT OK", APP_LTE_MQTT_CONN_TIMEOUT_MS) != ESP_OK) {
        /* 有的固件只回 OK 且已连上，再查一次 */
        if (!strstr(s_rx_acc, "OK")) {
            ESP_LOGE(TAG, "MIPSTART failed");
            return ESP_FAIL;
        }
        ESP_LOGW(TAG, "no CONNECT OK URC, continue");
    }

    snprintf(buf, sizeof(buf), "AT+MCONNECT=1,%d", APP_LTE_MQTT_KEEPALIVE_S);
    lte_uart_flush();
    s_rx_len = 0;
    s_rx_acc[0] = '\0';
    at_send_line(buf);
    if (at_wait_contains("CONNACK OK", 15000) != ESP_OK) {
        ESP_LOGE(TAG, "MCONNECT failed: %s", s_rx_acc);
        mqtt_close_quiet();
        return ESP_FAIL;
    }

    /* 订阅下行命令主题（可选） */
    snprintf(buf, sizeof(buf), "AT+MSUB=\"%s\",0", APP_LTE_MQTT_TOPIC_CMD);
    at_cmd_ok(buf, 5000);

    s_mqtt_up = true;
    ESP_LOGI(TAG, "MQTT up %s:%d id=%s", host, port, client_id);
    return ESP_OK;
}

esp_err_t lte_mqtt_disconnect(void)
{
    mqtt_close_quiet();
    return ESP_OK;
}

bool lte_mqtt_is_ready(void)
{
    return s_mqtt_up && s_powered;
}

esp_err_t lte_mqtt_publish(const char *topic, const char *payload)
{
    if (!topic || !payload) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_mqtt_up) {
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * AT+MPUB 载荷在双引号内；JSON 的 " 需写成 \"。
     * 超长则截断，避免撑爆 AT 行。
     */
    char esc[TELEMETRY_JSON_MAX + 64];
    size_t j = 0;
    for (size_t i = 0; payload[i] && j + 2 < sizeof(esc); i++) {
        char c = payload[i];
        if (c == '\\' || c == '"') {
            esc[j++] = '\\';
            esc[j++] = c;
        } else if ((unsigned char)c >= 0x20) {
            esc[j++] = c;
        }
    }
    esc[j] = '\0';

    char cmd[sizeof(esc) + 96];
    int n = snprintf(cmd, sizeof(cmd), "AT+MPUB=\"%s\",0,0,\"%s\"", topic, esc);
    if (n < 0 || n >= (int)sizeof(cmd)) {
        ESP_LOGE(TAG, "publish cmd too long");
        return ESP_ERR_NO_MEM;
    }

    if (at_cmd_ok(cmd, 8000) != ESP_OK) {
        ESP_LOGW(TAG, "MPUB fail, mark mqtt down");
        s_mqtt_up = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

void lte_poll(void)
{
    if (!s_uart_ok) {
        return;
    }
    uint8_t data[256];
    int n = uart_read_bytes(LTE_UART_NUM, data, sizeof(data) - 1, 0);
    if (n <= 0) {
        return;
    }
    data[n] = 0;
    ESP_LOGD(TAG, "rx: %s", (char *)data);

    /* 掉线 / 订阅下发 */
    if (strstr((char *)data, "MQTT DISCONNECT") ||
        strstr((char *)data, "CLOSED") ||
        strstr((char *)data, "ERROR")) {
        if (s_mqtt_up) {
            ESP_LOGW(TAG, "link lost indication");
            s_mqtt_up = false;
        }
    }
    /* +MSUB: "topic",len byte,payload — 交给上层以后扩展解析命令 */
    if (strstr((char *)data, "+MSUB:")) {
        ESP_LOGI(TAG, "cmd URC: %s", (char *)data);
    }
}
