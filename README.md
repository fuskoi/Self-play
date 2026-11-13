# Ridership — 双阵列 ToF 客流计数器固件

## 硬件定型

| 模块 | 型号 |
|------|------|
| 主控 | ESP32-S3-WROOM-1-N8R8 |
| ToF | VL53L5CX ×2 |
| 相机 | GC0308（触发式灰度） |
| 显示 | 0.96" SSD1306 OLED |
| 4G | Air780E |
| 电源 | ETA6003 + 3.3V Buck-Boost + 3000 mAh |

## 环境

- ESP-IDF **v5.1+**（推荐 v5.2/v5.3）
- 目标芯片：`esp32s3`

```bash
cd ridership
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

## 软件结构

```
main/
  app_main.c          启动与任务创建
  include/            公共头文件、引脚、配置
  algo/               分簇、跟踪、过线计数
  drivers/            ToF / 相机 / OLED / LTE AT / 按键蜂鸣
  tasks/              FreeRTOS 任务
  net/                JSON 协议、BLE/Wi-Fi/MQTT 路由
  power/              ACTIVE/STANDBY、电量
  ui/                 OLED 页面逻辑
```

## 说明

- `drivers/tof_vl53l5cx.c` 当前为 **仿真/桩实现**，便于无传感器时联调算法；接入官方 ULD 后替换测距读取即可。
- GC0308 / Air780E 驱动为可编译骨架，需按实际接线补全引脚与 AT 表。
- 默认引脚见 `include/board_pins.h`，与 PCB 不一致时只改该文件。