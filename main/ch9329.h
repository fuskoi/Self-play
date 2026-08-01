#pragma once

#include "driver/uart.h"
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#define CH9329_UART_NUM      UART_NUM_2
#define CH9329_TX_GPIO       17
#define CH9329_RX_GPIO       16
/* Factory default is often 9600; init will also try 115200. */
#define CH9329_BAUD          9600
#define CH9329_CHAR_DELAY_MS 25

esp_err_t ch9329_init(void);

/* Type UTF-8 text via USB HID keyboard.
 * ASCII uses normal keycodes; Chinese uses Windows Alt+GBK decimal (numpad). */
esp_err_t ch9329_type_utf8(const uint8_t *data, size_t len);

static inline esp_err_t ch9329_type_ascii(const uint8_t *data, size_t len)
{
    return ch9329_type_utf8(data, len);
}

esp_err_t ch9329_key_combo(uint8_t modifier, uint8_t keycode);
