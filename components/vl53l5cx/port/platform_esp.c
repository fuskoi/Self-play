/*******************************************************************************
 * VL53L5CX ULD 的 ESP-IDF I2C / 延时移植层（IDF v6 i2c_master）
 * 实际总线收发由 main/drivers/i2c_bus.c 提供。
 ******************************************************************************/
#include "platform.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define I2C_TIMEOUT_MS 1000
#define I2C_CHUNK      256

/* 由 main 强符号实现；单独编译本组件时弱符号兜底失败 */
esp_err_t __attribute__((weak)) i2c_bus_write(uint8_t addr7, const uint8_t *data, size_t len, int timeout_ms)
{
    (void)addr7;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t __attribute__((weak)) i2c_bus_write_read(uint8_t addr7, const uint8_t *wdata, size_t wlen,
                                                   uint8_t *rdata, size_t rlen, int timeout_ms)
{
    (void)addr7;
    (void)wdata;
    (void)wlen;
    (void)rdata;
    (void)rlen;
    (void)timeout_ms;
    return ESP_ERR_NOT_SUPPORTED;
}

static int32_t esp_get_tick(void)
{
    return (int32_t)(esp_timer_get_time() / 1000LL);
}

static int32_t esp_i2c_write(uint16_t address_8bit, uint16_t reg, uint8_t *data, uint16_t size)
{
    if (!data && size) {
        return 1;
    }
    uint8_t addr7 = (uint8_t)(address_8bit >> 1);
    uint16_t offset = 0;
    while (offset < size || (size == 0 && offset == 0)) {
        uint16_t chunk = (uint16_t)(size - offset);
        if (chunk > I2C_CHUNK) {
            chunk = I2C_CHUNK;
        }
        uint16_t reg_cur = (uint16_t)(reg + offset);

        uint8_t buf[2 + I2C_CHUNK];
        buf[0] = (uint8_t)(reg_cur >> 8);
        buf[1] = (uint8_t)(reg_cur & 0xFF);
        if (chunk) {
            memcpy(buf + 2, data + offset, chunk);
        }
        esp_err_t err = i2c_bus_write(addr7, buf, (size_t)(2 + chunk), I2C_TIMEOUT_MS);
        if (err != ESP_OK) {
            return (int32_t)err;
        }
        offset = (uint16_t)(offset + chunk);
        if (size == 0) {
            break;
        }
    }
    return 0;
}

static int32_t esp_i2c_read(uint16_t address_8bit, uint16_t reg, uint8_t *data, uint16_t size)
{
    if (!data || size == 0) {
        return 1;
    }
    uint8_t addr7 = (uint8_t)(address_8bit >> 1);
    uint16_t offset = 0;
    while (offset < size) {
        uint16_t chunk = (uint16_t)(size - offset);
        if (chunk > I2C_CHUNK) {
            chunk = I2C_CHUNK;
        }
        uint16_t reg_cur = (uint16_t)(reg + offset);
        uint8_t reg_bytes[2] = {
            (uint8_t)(reg_cur >> 8),
            (uint8_t)(reg_cur & 0xFF),
        };
        esp_err_t err = i2c_bus_write_read(addr7, reg_bytes, 2, data + offset, chunk, I2C_TIMEOUT_MS);
        if (err != ESP_OK) {
            return (int32_t)err;
        }
        offset = (uint16_t)(offset + chunk);
    }
    return 0;
}

void vl53l5cx_esp_platform_init(VL53L5CX_Platform *p_platform, uint16_t i2c_address_8bit)
{
    p_platform->address = i2c_address_8bit;
    p_platform->Write = esp_i2c_write;
    p_platform->Read = esp_i2c_read;
    p_platform->GetTick = esp_get_tick;
}

uint8_t RdByte(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t *p_value)
{
    return (uint8_t)p_platform->Read(p_platform->address, RegisterAdress, p_value, 1U);
}

uint8_t WrByte(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t value)
{
    return (uint8_t)p_platform->Write(p_platform->address, RegisterAdress, &value, 1U);
}

uint8_t WrMulti(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t *p_values, uint32_t size)
{
    while (size > 0) {
        uint16_t chunk = (size > 65535U) ? 65535U : (uint16_t)size;
        int32_t st = p_platform->Write(p_platform->address, RegisterAdress, p_values, chunk);
        if (st != 0) {
            return (uint8_t)st;
        }
        p_values += chunk;
        RegisterAdress = (uint16_t)(RegisterAdress + chunk);
        size -= chunk;
    }
    return 0;
}

uint8_t RdMulti(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t *p_values, uint32_t size)
{
    while (size > 0) {
        uint16_t chunk = (size > 65535U) ? 65535U : (uint16_t)size;
        int32_t st = p_platform->Read(p_platform->address, RegisterAdress, p_values, chunk);
        if (st != 0) {
            return (uint8_t)st;
        }
        p_values += chunk;
        RegisterAdress = (uint16_t)(RegisterAdress + chunk);
        size -= chunk;
    }
    return 0;
}

void SwapBuffer(uint8_t *buffer, uint16_t size)
{
    for (uint32_t i = 0; i < size; i += 4) {
        uint32_t tmp = ((uint32_t)buffer[i] << 24) | ((uint32_t)buffer[i + 1] << 16) |
                       ((uint32_t)buffer[i + 2] << 8) | (uint32_t)buffer[i + 3];
        memcpy(&buffer[i], &tmp, 4);
    }
}

uint8_t WaitMs(VL53L5CX_Platform *p_platform, uint32_t TimeMs)
{
    (void)p_platform;
    if (TimeMs == 0) {
        return 0;
    }
    vTaskDelay(pdMS_TO_TICKS(TimeMs));
    return 0;
}
