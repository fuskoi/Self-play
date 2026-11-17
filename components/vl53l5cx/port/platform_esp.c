/*******************************************************************************
 * VL53L5CX ULD 的 ESP-IDF I2C / 延时移植层
 ******************************************************************************/
#include "platform.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* 须与 main/include/board_pins.h 中 I2C_PORT_NUM 一致 */
#ifndef VL53L5CX_I2C_PORT
#define VL53L5CX_I2C_PORT I2C_NUM_0
#endif

#define I2C_TIMEOUT_MS 1000
#define I2C_CHUNK      256

/* main 里 i2c_bus.c 提供强符号；未链接时为空操作 */
void __attribute__((weak)) i2c_bus_lock(void) {}
void __attribute__((weak)) i2c_bus_unlock(void) {}

static int32_t esp_get_tick(void)
{
    return (int32_t)(esp_timer_get_time() / 1000LL);
}

static int32_t esp_i2c_write(uint16_t address_8bit, uint16_t reg, uint8_t *data, uint16_t size)
{
    if (!data && size) {
        return 1;
    }
    i2c_bus_lock();
    uint8_t addr7 = (uint8_t)(address_8bit >> 1);
    uint16_t offset = 0;
    while (offset < size || (size == 0 && offset == 0)) {
        uint16_t chunk = (uint16_t)(size - offset);
        if (chunk > I2C_CHUNK) {
            chunk = I2C_CHUNK;
        }
        uint16_t reg_cur = (uint16_t)(reg + offset);

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        if (!cmd) {
            return 1;
        }
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (uint8_t)((addr7 << 1) | I2C_MASTER_WRITE), true);
        i2c_master_write_byte(cmd, (uint8_t)(reg_cur >> 8), true);
        i2c_master_write_byte(cmd, (uint8_t)(reg_cur & 0xFF), true);
        if (chunk) {
            i2c_master_write(cmd, data + offset, chunk, true);
        }
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(VL53L5CX_I2C_PORT, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
        i2c_cmd_link_delete(cmd);
        if (err != ESP_OK) {
            i2c_bus_unlock();
            return (int32_t)err;
        }
        offset = (uint16_t)(offset + chunk);
        if (size == 0) {
            break;
        }
    }
    i2c_bus_unlock();
    return 0;
}

static int32_t esp_i2c_read(uint16_t address_8bit, uint16_t reg, uint8_t *data, uint16_t size)
{
    if (!data || size == 0) {
        return 1;
    }
    i2c_bus_lock();
    uint8_t addr7 = (uint8_t)(address_8bit >> 1);
    uint16_t offset = 0;
    while (offset < size) {
        uint16_t chunk = (uint16_t)(size - offset);
        if (chunk > I2C_CHUNK) {
            chunk = I2C_CHUNK;
        }
        uint16_t reg_cur = (uint16_t)(reg + offset);

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        if (!cmd) {
            return 1;
        }
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (uint8_t)((addr7 << 1) | I2C_MASTER_WRITE), true);
        i2c_master_write_byte(cmd, (uint8_t)(reg_cur >> 8), true);
        i2c_master_write_byte(cmd, (uint8_t)(reg_cur & 0xFF), true);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (uint8_t)((addr7 << 1) | I2C_MASTER_READ), true);
        if (chunk > 1) {
            i2c_master_read(cmd, data + offset, chunk - 1, I2C_MASTER_ACK);
        }
        i2c_master_read_byte(cmd, data + offset + chunk - 1, I2C_MASTER_NACK);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(VL53L5CX_I2C_PORT, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
        i2c_cmd_link_delete(cmd);
        if (err != ESP_OK) {
            i2c_bus_unlock();
            return (int32_t)err;
        }
        offset = (uint16_t)(offset + chunk);
    }
    i2c_bus_unlock();
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
    /* API 理论上可能传入 >65535；由下方分片 + esp_i2c_write 内部再切块 */
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
