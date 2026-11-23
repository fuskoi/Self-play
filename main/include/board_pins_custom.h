#pragma once

#include "hal/adc_types.h"

/*
 * 自研板引脚（对齐 docs/4.pdf）
 * 4G_RST 仅座子预留、不进 MCU；复位靠 EN 断电 + AT。
 */

/* I²C0：双 ToF + OLED */
#define PIN_I2C_SDA           6
#define PIN_I2C_SCL           5

/* ToF */
#define PIN_TOF_A_XSHUT       44  /* LPN1，模组脚 RXD0/GPIO44 */
#define PIN_TOF_B_XSHUT       7   /* LPN2 */
#define PIN_TOF_INT           43  /* 共网 TOF_INT，模组脚 TXD0/GPIO43 */

/* 4G：仅 EN + UART；无 RST */
#define PIN_LTE_TX            17  /* ESP→模组，网名 4G_RX */
#define PIN_LTE_RX            16  /* 模组→ESP，网名 4G_TX */
#define PIN_LTE_PWR_EN        4   /* 4G_EN */
#define PIN_LTE_RST           (-1)
#define PIN_LTE_PWRKEY        PIN_LTE_RST

#define PIN_CAM_PWR_EN        18  /* CAM_CTRL */

/* GC0308 DVP（8bit=Y2..Y9） */
#define PIN_CAM_XCLK          47  /* MCIK */
#define PIN_CAM_PCLK          13
#define PIN_CAM_VSYNC         39
#define PIN_CAM_HREF          45
#define PIN_CAM_SCCB_SDA      42
#define PIN_CAM_SCCB_SCL      41
#define PIN_CAM_D0            11  /* Y2 */
#define PIN_CAM_D1            9   /* Y3 */
#define PIN_CAM_D2            46  /* Y4 */
#define PIN_CAM_D3            10  /* Y5 */
#define PIN_CAM_D4            12  /* Y6 */
#define PIN_CAM_D5            14  /* Y7 */
#define PIN_CAM_D6            21  /* Y8 */
#define PIN_CAM_D7            48  /* Y9 */
#define PIN_CAM_PWDN          38
#define PIN_CAM_RESET         40

#define PIN_BUZZER            15  /* BUZZ_EN */
#define PIN_KEY_ADC           2
#define PIN_BAT_ADC           1
#define PIN_CHAG_EN           (-1) /* 本版未进 MCU */

#define PIN_KEY1              PIN_KEY_ADC
#define PIN_KEY2              PIN_KEY_ADC

#define I2C_PORT_NUM          I2C_NUM_0
#define I2C_FREQ_HZ           400000

#define LTE_UART_NUM          UART_NUM_1
#define LTE_UART_BAUD         115200

#define BAT_ADC_CHANNEL       ADC_CHANNEL_0  /* GPIO1 */
#define KEY_ADC_CHANNEL       ADC_CHANNEL_1  /* GPIO2 */
