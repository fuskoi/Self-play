#pragma once

#include "hal/adc_types.h"

/*
 * ESP32-S3-CAM 整机联调引脚（BOARD_S3_BRINGUP）
 *
 * 相机：按常见 Freenove / 同类 S3-CAM 板载 DVP（与自研板 custom 脚不同）
 * ToF+OLED：飞线到空闲脚，避开摄像头占用的 4～18
 *
 * 注意：板载 CAM 占用 GPIO16/17/18，4G 改到 41/42/47，勿再占 16～18。
 *
 * 飞线：
 *   OLED/ToF SDA -> GPIO21    SCL -> GPIO14
 *   ToF-A XSHUT  -> GPIO38
 *   ToF-B XSHUT  -> GPIO39
 *   ToF INT      -> GPIO40
 *   蜂鸣器       -> GPIO1
 */

/* I²C0：双 ToF + OLED */
#define PIN_I2C_SDA           21
#define PIN_I2C_SCL           14

/* ToF */
#define PIN_TOF_A_XSHUT       38
#define PIN_TOF_B_XSHUT       39
#define PIN_TOF_INT           40

/* 4G：避开板载 CAM 的 16/17/18 */
#define PIN_LTE_TX            41  /* ESP→模组 */
#define PIN_LTE_RX            42  /* 模组→ESP */
#define PIN_LTE_PWR_EN        47
#define PIN_LTE_RST           (-1)
#define PIN_LTE_PWRKEY        PIN_LTE_RST

/* 板载 CAM 常电；无独立 CAM_CTRL */
#define PIN_CAM_PWR_EN        (-1)

/* Freenove ESP32-S3-CAM 类 DVP（Y2..Y9 = D0..D7） */
#define PIN_CAM_XCLK          15
#define PIN_CAM_PCLK          13
#define PIN_CAM_VSYNC         6
#define PIN_CAM_HREF          7
#define PIN_CAM_SCCB_SDA      4
#define PIN_CAM_SCCB_SCL      5
#define PIN_CAM_D0            11  /* Y2 */
#define PIN_CAM_D1            9   /* Y3 */
#define PIN_CAM_D2            8   /* Y4 */
#define PIN_CAM_D3            10  /* Y5 */
#define PIN_CAM_D4            12  /* Y6 */
#define PIN_CAM_D5            18  /* Y7 */
#define PIN_CAM_D6            17  /* Y8 */
#define PIN_CAM_D7            16  /* Y9 */
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
