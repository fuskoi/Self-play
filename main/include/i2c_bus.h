#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

/* I2C0 总线：ToF + OLED 共用（IDF v6 新驱动） */
esp_err_t i2c_bus_ensure_init(void);
i2c_master_bus_handle_t i2c_bus_handle(void);

/** 7-bit 地址写 */
esp_err_t i2c_bus_write(uint8_t addr7, const uint8_t *data, size_t len, int timeout_ms);

/** 7-bit 地址：先写后读（repeated start） */
esp_err_t i2c_bus_write_read(uint8_t addr7, const uint8_t *wdata, size_t wlen,
                             uint8_t *rdata, size_t rlen, int timeout_ms);

void i2c_bus_lock(void);
void i2c_bus_unlock(void);
