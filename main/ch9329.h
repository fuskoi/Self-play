#pragma once

#include "driver/uart.h"
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#define CH9329_UART_NUM      UART_NUM_2
#define CH9329_TX_GPIO       17
#define CH9329_RX_GPIO       16
#define CH9329_BAUD          9600

/* ASCII mode: delay after each character (ms). Must be > chip packet interval. */
#define CH9329_ASCII_GAP_MS  12
/* Protocol Alt+GBK fallback report gap (ms). */
#define CH9329_ALT_GAP_MS    12

esp_err_t ch9329_init(void);

/* UTF-8 in → USB keyboard out.
 * Prefers CH9329 ASCII/GBK mode (fast). Falls back to Alt+GBK. */
esp_err_t ch9329_type_utf8(const uint8_t *data, size_t len);

static inline esp_err_t ch9329_type_ascii(const uint8_t *data, size_t len)
{
    return ch9329_type_utf8(data, len);
}

esp_err_t ch9329_key_combo(uint8_t modifier, uint8_t keycode);
