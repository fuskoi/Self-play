#include "ch9329.h"
#include "uni2gbk_table.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "ch9329";

#define CH9329_HEAD0     0x57
#define CH9329_HEAD1     0xAB
#define CH9329_ADDR      0x00
#define CMD_GET_INFO     0x01
#define CMD_SEND_KB      0x02
#define CMD_GET_PARA_CFG 0x08
#define CMD_SET_PARA_CFG 0x09
#define CMD_RESET        0x0F
#define PARA_LEN         50

#define HID_MOD_LALT 0x04
#define HID_KP_1     0x59
#define HID_KP_0     0x62

static int s_baud = CH9329_BAUD;
static bool s_chip_ok;
static bool s_ascii_mode;

static uint8_t ascii_to_hid(char c, uint8_t *modifier)
{
    *modifier = 0;
    if (c >= 'a' && c <= 'z') {
        return (uint8_t)(0x04 + (c - 'a'));
    }
    if (c >= 'A' && c <= 'Z') {
        *modifier = 0x02;
        return (uint8_t)(0x04 + (c - 'A'));
    }
    if (c >= '1' && c <= '9') {
        return (uint8_t)(0x1E + (c - '1'));
    }
    if (c == '0') {
        return 0x27;
    }
    switch (c) {
    case '\n':
    case '\r':
        return 0x28;
    case '\t':
        return 0x2B;
    case ' ':
        return 0x2C;
    case '-':
        return 0x2D;
    case '=':
        return 0x2E;
    case '[':
        return 0x2F;
    case ']':
        return 0x30;
    case '\\':
        return 0x31;
    case ';':
        return 0x33;
    case '\'':
        return 0x34;
    case '`':
        return 0x35;
    case ',':
        return 0x36;
    case '.':
        return 0x37;
    case '/':
        return 0x38;
    case '!':
        *modifier = 0x02;
        return 0x1E;
    case '@':
        *modifier = 0x02;
        return 0x1F;
    case '#':
        *modifier = 0x02;
        return 0x20;
    case '$':
        *modifier = 0x02;
        return 0x21;
    case '%':
        *modifier = 0x02;
        return 0x22;
    case '^':
        *modifier = 0x02;
        return 0x23;
    case '&':
        *modifier = 0x02;
        return 0x24;
    case '*':
        *modifier = 0x02;
        return 0x25;
    case '(':
        *modifier = 0x02;
        return 0x26;
    case ')':
        *modifier = 0x02;
        return 0x27;
    case '_':
        *modifier = 0x02;
        return 0x2D;
    case '+':
        *modifier = 0x02;
        return 0x2E;
    case '{':
        *modifier = 0x02;
        return 0x2F;
    case '}':
        *modifier = 0x02;
        return 0x30;
    case '|':
        *modifier = 0x02;
        return 0x31;
    case ':':
        *modifier = 0x02;
        return 0x33;
    case '"':
        *modifier = 0x02;
        return 0x34;
    case '~':
        *modifier = 0x02;
        return 0x35;
    case '<':
        *modifier = 0x02;
        return 0x36;
    case '>':
        *modifier = 0x02;
        return 0x37;
    case '?':
        *modifier = 0x02;
        return 0x38;
    default:
        return 0;
    }
}

static uint8_t digit_to_keypad(char d)
{
    if (d == '0') {
        return HID_KP_0;
    }
    if (d >= '1' && d <= '9') {
        return (uint8_t)(HID_KP_1 + (d - '1'));
    }
    return 0;
}

static esp_err_t ch9329_send_packet(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    uint8_t pkt[2 + 1 + 1 + 1 + 64 + 1];
    if (len > 64) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t i = 0;
    pkt[i++] = CH9329_HEAD0;
    pkt[i++] = CH9329_HEAD1;
    pkt[i++] = CH9329_ADDR;
    pkt[i++] = cmd;
    pkt[i++] = len;
    if (len && data) {
        memcpy(&pkt[i], data, len);
        i += len;
    }

    uint16_t sum = 0;
    for (size_t s = 0; s < i; ++s) {
        sum += pkt[s];
    }
    pkt[i++] = (uint8_t)(sum & 0xFF);

    int n = uart_write_bytes(CH9329_UART_NUM, (const char *)pkt, i);
    return (n == (int)i) ? ESP_OK : ESP_FAIL;
}

static int ch9329_read_reply(uint8_t expect_cmd, uint8_t *data, int data_cap, int timeout_ms)
{
    uint8_t buf[80];
    int n = uart_read_bytes(CH9329_UART_NUM, buf, sizeof(buf), pdMS_TO_TICKS(timeout_ms));
    if (n < 6) {
        return -1;
    }
    for (int i = 0; i + 5 < n; ++i) {
        if (buf[i] != CH9329_HEAD0 || buf[i + 1] != CH9329_HEAD1) {
            continue;
        }
        if (buf[i + 3] != expect_cmd) {
            continue;
        }
        int len = buf[i + 4];
        if (i + 5 + len >= n) {
            return -1;
        }
        if (len > data_cap) {
            len = data_cap;
        }
        if (data && len > 0) {
            memcpy(data, &buf[i + 5], len);
        }
        return len;
    }
    return -1;
}

static bool ch9329_probe(void)
{
    uart_flush_input(CH9329_UART_NUM);
    if (ch9329_send_packet(CMD_GET_INFO, NULL, 0) != ESP_OK) {
        return false;
    }
    uint8_t data[16];
    return ch9329_read_reply(0x81, data, sizeof(data), 150) > 0;
}

static esp_err_t ch9329_send_report(const uint8_t report[8])
{
    esp_err_t err = ch9329_send_packet(CMD_SEND_KB, report, 8);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(CH9329_ALT_GAP_MS));
    return ESP_OK;
}

static esp_err_t ch9329_send_key(uint8_t modifier, uint8_t keycode)
{
    uint8_t report[8] = {0};
    report[0] = modifier;
    report[2] = keycode;
    esp_err_t err = ch9329_send_report(report);
    if (err != ESP_OK) {
        return err;
    }
    memset(report, 0, sizeof(report));
    return ch9329_send_report(report);
}

static size_t utf8_decode(const uint8_t *data, size_t len, uint32_t *out_cp)
{
    if (len == 0) {
        return 0;
    }
    uint8_t b0 = data[0];
    if (b0 < 0x80) {
        *out_cp = b0;
        return 1;
    }
    if ((b0 & 0xE0) == 0xC0) {
        if (len < 2 || (data[1] & 0xC0) != 0x80) {
            return 0;
        }
        *out_cp = ((uint32_t)(b0 & 0x1F) << 6) | (data[1] & 0x3F);
        return 2;
    }
    if ((b0 & 0xF0) == 0xE0) {
        if (len < 3 || (data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80) {
            return 0;
        }
        *out_cp = ((uint32_t)(b0 & 0x0F) << 12) | ((uint32_t)(data[1] & 0x3F) << 6) |
                  (data[2] & 0x3F);
        return 3;
    }
    if ((b0 & 0xF8) == 0xF0) {
        if (len < 4 || (data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80 ||
            (data[3] & 0xC0) != 0x80) {
            return 0;
        }
        *out_cp = ((uint32_t)(b0 & 0x07) << 18) | ((uint32_t)(data[1] & 0x3F) << 12) |
                  ((uint32_t)(data[2] & 0x3F) << 6) | (data[3] & 0x3F);
        return 4;
    }
    return 0;
}

static uint16_t unicode_to_gbk(uint32_t cp)
{
    if (cp > 0xFFFF) {
        return 0;
    }
    uint16_t uni = (uint16_t)cp;
    int lo = 0;
    int hi = UNI2GBK_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint16_t u = UNI2GBK_TABLE[mid].uni;
        if (u == uni) {
            return UNI2GBK_TABLE[mid].gbk;
        }
        if (u < uni) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return 0;
}

static esp_err_t ch9329_type_gbk_alt(uint16_t gbk)
{
    char digits[12];
    int n = snprintf(digits, sizeof(digits), "%u", (unsigned)gbk);
    if (n <= 0 || n >= (int)sizeof(digits)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t report[8] = {0};
    report[0] = HID_MOD_LALT;
    esp_err_t err = ch9329_send_report(report);
    if (err != ESP_OK) {
        return err;
    }

    for (int i = 0; i < n; ++i) {
        uint8_t kp = digit_to_keypad(digits[i]);
        if (kp == 0) {
            continue;
        }
        report[0] = HID_MOD_LALT;
        report[2] = kp;
        err = ch9329_send_report(report);
        if (err != ESP_OK) {
            return err;
        }
        report[2] = 0;
        err = ch9329_send_report(report);
        if (err != ESP_OK) {
            return err;
        }
    }

    memset(report, 0, sizeof(report));
    return ch9329_send_report(report);
}

/*
 * Switch to ASCII serial mode for native GBK/区位码 typing.
 * Critical: fast-upload OFF; each Han char sent as one 2-byte UART write.
 */
static bool ch9329_enable_ascii_mode(void)
{
    uart_flush_input(CH9329_UART_NUM);
    if (ch9329_send_packet(CMD_GET_PARA_CFG, NULL, 0) != ESP_OK) {
        return false;
    }

    uint8_t para[PARA_LEN];
    if (ch9329_read_reply(0x88, para, PARA_LEN, 300) < PARA_LEN) {
        ESP_LOGW(TAG, "GET_PARA_CFG failed");
        return false;
    }

    para[0] = (uint8_t)(para[0] & 0x03);
    if (para[0] > 1) {
        para[0] = 0x01; /* keyboard only */
    }
    para[1] = 0x01; /* ASCII mode */
    para[9] = 0x00;
    para[10] = 8;   /* UART frame gap 8ms — bytes of one char must arrive faster */
    para[15] = 0x00;
    para[16] = 0x00; /* key upload interval 0 */
    para[17] = 0x00;
    para[18] = 0x01; /* key release delay 1ms */
    para[19] = 0x00; /* no auto enter */
    para[37] = 0x00; /* fast upload OFF (ON causes 你好→媚愫 desync) */

    uart_flush_input(CH9329_UART_NUM);
    if (ch9329_send_packet(CMD_SET_PARA_CFG, para, PARA_LEN) != ESP_OK) {
        return false;
    }
    uint8_t ack[4];
    if (ch9329_read_reply(0x89, ack, sizeof(ack), 300) < 1) {
        ESP_LOGW(TAG, "SET_PARA_CFG no ack");
        return false;
    }

    ch9329_send_packet(CMD_RESET, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    uart_flush_input(CH9329_UART_NUM);
    ESP_LOGI(TAG, "ASCII/GBK mode ready (fast upload off)");
    return true;
}

/* One character per write: 1 byte ASCII or 2 bytes GBK together. */
static esp_err_t ascii_put_char(const uint8_t *bytes, size_t n)
{
    if (n == 0) {
        return ESP_OK;
    }
    if (uart_write_bytes(CH9329_UART_NUM, (const char *)bytes, n) != (int)n) {
        return ESP_FAIL;
    }
    vTaskDelay(pdMS_TO_TICKS(CH9329_ASCII_GAP_MS));
    return ESP_OK;
}

esp_err_t ch9329_init(void)
{
    uart_config_t cfg = {
        .baud_rate = CH9329_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(CH9329_UART_NUM, 1024, 256, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CH9329_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(CH9329_UART_NUM, CH9329_TX_GPIO, CH9329_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    static const int baud_try[] = {9600, 115200};
    s_chip_ok = false;
    s_ascii_mode = false;

    for (size_t i = 0; i < sizeof(baud_try) / sizeof(baud_try[0]); ++i) {
        s_baud = baud_try[i];
        ESP_ERROR_CHECK(uart_set_baudrate(CH9329_UART_NUM, s_baud));
        vTaskDelay(pdMS_TO_TICKS(20));
        ESP_LOGI(TAG, "probing at %d baud...", s_baud);
        if (ch9329_probe()) {
            s_chip_ok = true;
            break;
        }
    }

    if (s_chip_ok) {
        if (ch9329_enable_ascii_mode()) {
            s_ascii_mode = true;
            ESP_LOGI(TAG, "ready: ASCII/GBK fast path @ %d", s_baud);
        } else {
            ESP_LOGW(TAG, "ASCII switch failed; slow Alt+GBK fallback @ %d", s_baud);
        }
    } else {
        /* Previously configured ASCII — protocol probe will fail. */
        s_baud = 9600;
        ESP_ERROR_CHECK(uart_set_baudrate(CH9329_UART_NUM, s_baud));
        s_chip_ok = true;
        s_ascii_mode = true;
        ESP_LOGW(TAG, "no protocol reply → assume ASCII @ %d (pull SET+GND to reconfigure)", s_baud);
    }
    return ESP_OK;
}

esp_err_t ch9329_key_combo(uint8_t modifier, uint8_t keycode)
{
    if (s_ascii_mode) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ch9329_send_key(modifier, keycode);
}

esp_err_t ch9329_type_utf8(const uint8_t *data, size_t len)
{
    size_t i = 0;
    while (i < len) {
        uint32_t cp = 0;
        size_t n = utf8_decode(data + i, len - i, &cp);
        if (n == 0) {
            i++;
            continue;
        }
        i += n;

        esp_err_t err = ESP_OK;
        if (s_ascii_mode) {
            if (cp < 0x80) {
                uint8_t b = (uint8_t)cp;
                err = ascii_put_char(&b, 1);
            } else {
                uint16_t gbk = unicode_to_gbk(cp);
                if (gbk == 0) {
                    ESP_LOGW(TAG, "no GBK for U+%04lX", (unsigned long)cp);
                    continue;
                }
                uint8_t pair[2] = {(uint8_t)(gbk >> 8), (uint8_t)(gbk & 0xFF)};
                err = ascii_put_char(pair, 2); /* MUST be one write */
            }
        } else if (cp < 0x80) {
            uint8_t mod = 0;
            uint8_t key = ascii_to_hid((char)cp, &mod);
            if (key == 0) {
                continue;
            }
            err = ch9329_send_key(mod, key);
        } else {
            uint16_t gbk = unicode_to_gbk(cp);
            if (gbk == 0) {
                continue;
            }
            err = ch9329_type_gbk_alt(gbk);
        }

        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}
