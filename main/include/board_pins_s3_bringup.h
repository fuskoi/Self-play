#pragma once

#include "hal/adc_types.h"

/*
 * ESP32-S3-CAM / DevKit 杜邦飞线联调引脚
 *
 * 目标：在现成 S3 板上先跑通 ToF×2 + OLED + 算法/联网；
 * 板载摄像头脚位与自研 GC0308 不同，默认不接 CAM（走 stub）。
 *
 * 默认按「避开常见 CAM 占用脚」选了一组；若与你的板子冲突，
 * 只改本文件，勿改 custom。杜邦线按丝印核对后再焊。
 *
 * 推荐接线（3V3 共地）：
 *   OLED/ToF SDA -> GPIO21    SCL -> GPIO14
 *   ToF-A LPN/XSHUT -> GPIO38
 *   ToF-B LPN/XSHUT -> GPIO39
 *   ToF INT（两路线或）-> GPIO40
 *   蜂鸣器 EN -> GPIO1（可选）
 *
 * 控制台请用 USB-Serial/JTAG，避免占用 UART0 的 43/44。
 */

/* I²C0：双 ToF + OLED（杜邦） */
#define PIN_I2C_SDA           21
#define PIN_I2C_SCL           14

/* ToF */
#define PIN_TOF_A_XSHUT       38
#define PIN_TOF_B_XSHUT       39
#define PIN_TOF_INT           40

/* 4G：有模组再飞线；暂无则保持脚号即可 */
#define PIN_LTE_TX            17
#define PIN_LTE_RX            16
#define PIN_LTE_PWR_EN        18
#define PIN_LTE_RST           (-1)
#define PIN_LTE_PWRKEY        PIN_LTE_RST

/* 开发板阶段默认不控相机电源；init 失败会走 stub */
#define PIN_CAM_PWR_EN        (-1)

#define PIN_CAM_XCLK          (-1)
#define PIN_CAM_PCLK          (-1)
#define PIN_CAM_VSYNC         (-1)
#define PIN_CAM_HREF          (-1)
#define PIN_CAM_SCCB_SDA      (-1)
#define PIN_CAM_SCCB_SCL      (-1)
#define PIN_CAM_D0            (-1)
#define PIN_CAM_D1            (-1)
#define PIN_CAM_D2            (-1)
#define PIN_CAM_D3            (-1)
#define PIN_CAM_D4            (-1)
#define PIN_CAM_D5            (-1)
#define PIN_CAM_D6            (-1)
#define PIN_CAM_D7            (-1)
#define PIN_CAM_PWDN          (-1)
#define PIN_CAM_RESET         (-1)

#define PIN_BUZZER            1
#define PIN_KEY_ADC           2
#define PIN_BAT_ADC           (-1)
#define PIN_CHAG_EN           (-1)

#define PIN_KEY1              PIN_KEY_ADC
#define PIN_KEY2              PIN_KEY_ADC

#define I2C_PORT_NUM          I2C_NUM_0
#define I2C_FREQ_HZ           400000

#define LTE_UART_NUM          UART_NUM_1
#define LTE_UART_BAUD         115200

#define BAT_ADC_CHANNEL       ADC_CHANNEL_0
#define KEY_ADC_CHANNEL       ADC_CHANNEL_1  /* GPIO2 */
