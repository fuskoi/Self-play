#pragma once

/* ESP32-S3-WROOM-1-N8R8 引脚（按方案建议，PCB 定稿后只改本文件） */

#define PIN_I2C_SDA           8
#define PIN_I2C_SCL           9

#define PIN_TOF_A_XSHUT       4
#define PIN_TOF_B_XSHUT       5
#define PIN_TOF_INT           3

#define PIN_LTE_TX            17
#define PIN_LTE_RX            18
#define PIN_LTE_PWR_EN        10
#define PIN_LTE_PWRKEY        11

#define PIN_CAM_PWR_EN        7
/* GC0308 DVP 脚位按模组与 esp32-camera 例程在 cam_gc0308.c 中配置 */

#define PIN_BUZZER            12
#define PIN_KEY1              0
#define PIN_KEY2              1
#define PIN_BAT_ADC           2

#define I2C_PORT_NUM          I2C_NUM_0
#define I2C_FREQ_HZ           400000

#define LTE_UART_NUM          UART_NUM_1
#define LTE_UART_BAUD         115200