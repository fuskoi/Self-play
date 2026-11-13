/*******************************************************************************
 * VL53L5CX Ultra Lite Driver 的 ESP-IDF 平台层
 * 基于 ST 官方 ULD platform.h（BSD-3-Clause / ST 双许可）
 ******************************************************************************/
#pragma once

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (*VL53L5CX_get_tick_Func)(void);
typedef int32_t (*VL53L5CX_write_Func)(uint16_t, uint16_t, uint8_t *, uint16_t);
typedef int32_t (*VL53L5CX_read_Func)(uint16_t, uint16_t, uint8_t *, uint16_t);

typedef struct {
    /* ST ULD 使用的 8 位 I2C 地址（默认 0x52） */
    uint16_t address;
    VL53L5CX_write_Func Write;
    VL53L5CX_read_Func Read;
    VL53L5CX_get_tick_Func GetTick;
} VL53L5CX_Platform;

#ifndef VL53L5CX_NB_TARGET_PER_ZONE
#define VL53L5CX_NB_TARGET_PER_ZONE (1U)
#endif

/* 关闭未用输出，减小 I2C 载荷与 RAM 占用 */
#define VL53L5CX_DISABLE_NB_SPADS_ENABLED
#define VL53L5CX_DISABLE_AMBIENT_DMAX
#define VL53L5CX_DISABLE_RANGE_SIGMA_MM
#define VL53L5CX_DISABLE_REFLECTANCE_PERCENT
#define VL53L5CX_DISABLE_MOTION_INDICATOR
#define VL53L5CX_DISABLE_MOTION_DETECTOR

#define PROCESSOR_LITTLE_ENDIAN
#ifdef PROCESSOR_LITTLE_ENDIAN
#define SWAP_UINT16(x) (x)
#define SWAP_UINT32(x) (x)
#else
#define SWAP_UINT16(x) (((x) >> 8) | ((x) << 8))
#define SWAP_UINT32(x) (((x) >> 24) | (((x) & 0x00FF0000) >> 8) \
    | (((x) & 0x0000FF00) << 8) | ((x) << 24))
#endif

uint8_t RdByte(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t *p_value);
uint8_t WrByte(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t value);
uint8_t RdMulti(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t *p_values, uint32_t size);
uint8_t WrMulti(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t *p_values, uint32_t size);
void SwapBuffer(uint8_t *buffer, uint16_t size);
uint8_t WaitMs(VL53L5CX_Platform *p_platform, uint32_t TimeMs);

/** 绑定 ESP-IDF 的 I2C 回调到平台结构（每个传感器调用一次） */
void vl53l5cx_esp_platform_init(VL53L5CX_Platform *p_platform, uint16_t i2c_address_8bit);

#ifdef __cplusplus
}
#endif
