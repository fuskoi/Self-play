#pragma once

#include "esp_err.h"

/* I2C0 总线：ToF + OLED 共用，事务前后加锁 */
esp_err_t i2c_bus_ensure_init(void);
void      i2c_bus_lock(void);
void      i2c_bus_unlock(void);
