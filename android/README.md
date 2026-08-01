# ESP Text HID — Android 客户端

配套 `esp32-bt-hid-bridge` 固件：手机通过经典蓝牙 SPP 发送 UTF-8 文本，ESP32 经 CH9329 模拟键盘输入到电脑光标处。

## 使用步骤

1. 用 Android Studio 打开本目录（`android/`）。
2. 连接真机（模拟器通常没有经典蓝牙），编译安装。
3. 系统设置里配对 **ESP-TEXT-KB**，PIN：`1234`。
4. 打开 App → 刷新 → 选中设备 → 连接 → 输入文字 → **发送并输入**。
5. 电脑上先点好输入框/光标位置。

## 协议

与固件 `frame_proto` 一致：

| 字段 | 说明 |
|------|------|
| 头 | `AA 55` |
| cmd | `01` DATA / `02` END / `03` ABORT；应答 `81` ACK / `82` NAK |
| seq | 大端 uint16，从 0 递增 |
| len | payload 长度，最大 1024 |
| crc | Modbus CRC16，覆盖 cmd…payload |

流程：分包 `DATA`（等 ACK）→ `END`（等 ACK）→ 固件开始打字。

## 注意

- 仅 Android；iOS 不支持经典蓝牙 SPP。
- 中文依赖 Windows 的 Alt+小键盘 Unicode 输入，需电脑侧可用。
- 单次文本上限 64KB（与固件缓冲一致）。
