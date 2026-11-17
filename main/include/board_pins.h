#pragma once

/* ESP32-S3-WROOM-1-N8R8 引脚（按方案建议，PCB 定稿后只改本文件） */

/* I²C0：双 ToF + SSD1306 OLED 共总线（模组地址默认 0x3C，冲突时驱动会试 0x3D） */
#define PIN_I2C_SDA           8
#define PIN_I2C_SCL           9

#define PIN_TOF_A_XSHUT       4
#define PIN_TOF_B_XSHUT       5
#define PIN_TOF_INT           3

/* 4G：ESP TX→模组 RXD，ESP RX←模组 TXD；PWR_EN 控 VIN/负载开关；PWRKEY 拉低约 1s 开机 */
#define PIN_LTE_TX            17
#define PIN_LTE_RX            18
#define PIN_LTE_PWR_EN        10
#define PIN_LTE_PWRKEY        11

#define PIN_CAM_PWR_EN        7
/*
 * GC0308 DVP（手册：SCCB 写 0x42/读 0x43，Chip_ID=0x9b；YCbCr/RGB565/Bayer）
 * 与 ToF/OLED I2C、LTE UART、按键错开；定稿 PCB 时只改本段。
 * 注意：GPIO45/46 为 Strapping，若模组走这两脚需核对上电时序。
 */
#define PIN_CAM_XCLK          15
#define PIN_CAM_PCLK          13
#define PIN_CAM_VSYNC         6
#define PIN_CAM_HREF          14
#define PIN_CAM_SCCB_SDA      21
#define PIN_CAM_SCCB_SCL      16
#define PIN_CAM_D0            42 /* Y2 */
#define PIN_CAM_D1            41
#define PIN_CAM_D2            40
#define PIN_CAM_D3            39
#define PIN_CAM_D4            38
#define PIN_CAM_D5            47
#define PIN_CAM_D6            48
#define PIN_CAM_D7            45 /* Y9；若冲突可改 46 并评估 strapping */
#define PIN_CAM_PWDN          (-1) /* 断电走 CAM_PWR_EN */
#define PIN_CAM_RESET         (-1)

#define PIN_BUZZER            12
#define PIN_KEY1              0
#define PIN_KEY2              1
#define PIN_BAT_ADC           2

#define I2C_PORT_NUM          I2C_NUM_0
#define I2C_FREQ_HZ           400000

#define LTE_UART_NUM          UART_NUM_1
#define LTE_UART_BAUD         115200