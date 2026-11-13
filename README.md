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

## 烧录后验收

严格按 `docs/验收测试手册.md` 执行（对齐软件方案 **§12 T1～T10**）。  
监视器中输入 `help` 可看串口调试命令（`status` / `in` / `thresh` / `bat_mv` / `standby` / `net` 等）。

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

- ToF：已移植 ST **VL53L5CX ULD**（`components/vl53l5cx`），`tof_vl53l5cx.c` 支持双传感器 XSHUT 改址；无硬件时自动回退仿真，串口可用 `sim on|off`。
- GC0308 / Air780E / OLED / BLE 仍为骨架，需接实物后补全。
- 默认引脚见 `include/board_pins.h`，与 PCB 不一致时只改该文件。
- Flash 使用自定义 `partitions.csv`（app 3MB），适配 ULD 固件体积。