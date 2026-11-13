# 基于双阵列 ToF 的双向客流计数器 · 软件技术方案

## 嘉立创星火计划 · 纯软件方案书（竞标用）

| 项目 | 内容 |
|------|------|
| 项目名称 | 双阵列 ToF 双向客流计数系统软件 |
| 配套硬件（已定型） | ESP32-S3-WROOM-1-N8R8；2×VL53L8CX（或 L5CX）；灰度相机；Cat.1；OLED；锂电池 |
| 软件范围 | 设备固件 + 近场/远程通信协议 + 轻量手机端/网页端 + 云端主题约定 |
| 运行框架 | ESP-IDF + FreeRTOS |
| 文档版本 | V1.0 |

> 本文只描述**会实现的软件**。算法、协议、任务、低功耗状态机均可在上述硬件上落地；不写未计划开发的大模型、原生复杂 App、录像云存等能力。

---

## 1. 软件目标与指标

| 目标 | 软件对应指标 |
|------|----------------|
| 计数准确率 ≥ 95% | 单门场景：单人、双人并行、双人并行+一人随后；多目标跟踪 + 触发式灰度校验 |
| 双向进出 / 在场 | 虚拟门线状态机；日累计进、出；`occ = max(0, in - out)` |
| 本地交互 | OLED 刷新进/出/在场/电量/联网；超员与低电蜂鸣 |
| 三网通信 | BLE 配网与实时 Notify；Wi-Fi MQTT；4G AT MQTT；统一 JSON |
| 低功耗 | 无人超时进 STANDBY；关 Wi-Fi/4G/相机；ToF 中断唤醒 |
| 工作续航 ≥ 3 h | ACTIVE 策略可控上报频率；非必要不常开相机/4G |

---

## 2. 总体软件架构

```
┌─────────────────────────────────────────────────────────────┐
│                     应用 / 产品层                            │
│  日计数 · 超员策略 · 低电策略 · 配网 · 模式切换(Wi-Fi/4G)    │
├─────────────────────────────────────────────────────────────┤
│                     业务服务层                               │
│  count_service · alarm_service · telemetry_service · ui     │
├──────────────┬──────────────┬──────────────┬────────────────┤
│  感知算法    │  电源管理    │  连接管理    │  存储         │
│  ToF 分簇    │  ACTIVE/     │  BLE/Wi-Fi/  │  NVS 参数     │
│  跟踪过线    │  STANDBY     │  LTE 路由    │  日计数持久化 │
│  相机校验    │  电量采样    │  MQTT 发布   │               │
├──────────────┴──────────────┴──────────────┴────────────────┤
│                     驱动 / 中间件（ESP-IDF）                 │
│  VL53Lx ULD · esp32-camera · SSD1306 · ADC · UART AT ·     │
│  NimBLE · esp_wifi · esp-mqtt · GPIO sleep                  │
└─────────────────────────────────────────────────────────────┘
```

**设计原则：**

1. **ToF 主路径、相机辅路径**：方向与轨迹只信双 ToF；相机只在多人嫌疑时回答「几个人」。
2. **一套数据模型、三通道发送**：业务只产出一份 telemetry，由连接层按当前链路发出。
3. **可演示、可裁剪**：手机端以 BLE + 网页/小程序为主，保证结项可看。

---

## 3. 进程 / 任务划分（FreeRTOS）

| 任务名 | 优先级 | 周期/触发 | 职责 |
|--------|--------|-----------|------|
| `tof_task` | 高 | 15～30 Hz（ACTIVE） | 读双 ToF、高度滤波、分簇、关联 track |
| `cam_task` | 中 | 事件（多人嫌疑） | 上电→采帧→连通域点数→断电 |
| `count_task` | 高 | 感知事件队列 | 虚拟线穿越、更新 in/out/occ、触发告警 |
| `ui_task` | 低 | 5～10 Hz | OLED、蜂鸣器模式、按键消抖 |
| `ble_task` | 中 | 连接事件 | GATT、配网写凭证、Notify |
| `wifi_mqtt_task` | 中 | 连接态 | STA、MQTT 连接与发布 |
| `lte_task` | 中 | 状态机 | 4G 上电、AT、注网、MQTT |
| `power_task` | 中 | 200 ms / 睡前 | 无人超时、电量、进退待机 |
| `telemetry_task` | 低 | 事件+心跳 | 组装 JSON，投递到当前通道 |

任务间用 **FreeRTOS Queue / EventGroup** 解耦，禁止在 ISR 里做重算法。

---

## 4. 感知与计数算法（核心）

### 4.1 坐标系与标定

- 安装俯视；软件保存：安装高度 `H`、门宽 ROI、单人簇宽度阈值 `W_single`、人高距离带。
- 上电或按键进入 **空场标定**：记录背景深度，后续做差分/门限。

### 4.2 ToF 流水线

```
深度矩阵(8×8 或 4×4)
  → 有效距离 / 信号质量过滤
  → 换算离地高度，保留人高区间
  → 横向连通域 / 分簇 → Cluster{cx, cy, width, height}
  → 与上一帧最近邻关联 → Track{id, history, side}
  → 输出 track 列表 + multi_suspect 标志
```

**多人嫌疑（触发相机）条件（满足其一即可）：**

1. 有效簇数量 ≥ 2；
2. 单簇宽度 > `k * W_single`（如 k=1.6）；
3. 纵向短时新出现第二/第三个 track（尾随）。

### 4.3 触发式灰度人数校验

```
if multi_suspect:
  cam_power_on()
  采集 1～3 帧 QQVGA 灰度（不存 Flash、不上传原图）
  简易阈值分割 + 连通域，统计 1～3 个类人头目标 → person_hint
  cam_power_off()
  将 person_hint 写入当前过线窗口的计数校正
```

单人通行不触发，相机保持断电。

### 4.4 双向过线状态机

对每个 `track_id` 独立维护侧别：`SIDE_A` / `SIDE_B` / `UNKNOWN`。

| 转移 | 动作 |
|------|------|
| A → 穿越虚拟线 → B | `in++`；更新 `occ` |
| B → 穿越虚拟线 → A | `out++`；更新 `occ` |
| 抖动来回未满足最小位移/时间 | 不计 |

**禁止**「整帧只有一个计数器」：双人并行、前面两人后面一人跟随，必须多 ID 分别过线。

防抖：最小穿越时间、冷却时间、短时丢检保持（200～500 ms）。

### 4.5 告警逻辑

```
if occ >= occ_threshold: alarm_overcrowd = 1 → 蜂鸣模式 A + telemetry
if vbat < V_LOW:        alarm_battery = 1 → 蜂鸣模式 B + UI + telemetry
if vbat < V_CRIT:       禁止拉起 4G，尽快 STANDBY
```

阈值与滞回参数存 NVS，可通过 BLE/MQTT 下发修改。

---

## 5. 低功耗软件状态机

```
              T_idle 无人
   ACTIVE ──────────────────► STANDBY
      ▲                         │
      │  ToF INT / 按键         │ Light Sleep
      └─────────────────────────┘
```

### 5.1 进入 STANDBY（`enter_standby`）

1. 停 telemetry 高频发布；OLED 熄屏；蜂鸣停。  
2. `esp_wifi_stop()`；BLE 关闭或超长间隔（演示省电可关）。  
3. `lte_power_cut()`；`cam_power_cut()`。  
4. ToF 切自主低频测距 + 距离阈值中断。  
5. 配置 GPIO 唤醒：`TOF_INT`、按键。  
6. `esp_light_sleep_start()`（首版用浅睡，RAM 中保留计数；关键计数同时写 NVS）。

### 5.2 退出 STANDBY（`exit_standby`）

1. ToF 恢复高速测距；确认确有人体目标（防误唤醒）。  
2. 恢复 BLE；按配置恢复 Wi-Fi 或 4G。  
3. 相机保持关，仅多人嫌疑再开。  
4. 回到 ACTIVE。

### 5.3 无人判定

`last_detect_ms` 在存在有效人体 track 时刷新；`now - last_detect_ms > T_IDLE`（默认 15 s，可配置）则进入待机。

---

## 6. 通信软件方案

### 6.1 统一数据模型

**上报 telemetry（设备 → 手机/云）：**

```json
{
  "device_id": "pcounter-xxxxxx",
  "in": 12,
  "out": 9,
  "occ": 3,
  "bat": 76,
  "alarm": 0,
  "net": "wifi",
  "ts": 1731384000
}
```

`alarm` 位定义示例：bit0 超员，bit1 低电。

**下行 command（手机/云 → 设备）：**

```json
{
  "cmd": "set_threshold",
  "occ_threshold": 50
}
```

支持命令：`set_threshold`、`clear_daily`、`set_net_mode`（`wifi`/`lte`/`auto`）、`set_idle_sec`、`get_status`。

### 6.2 BLE（近场）

| 内容 | 说明 |
|------|------|
| 角色 | GATT Server |
| 用途 | 配网（写 SSID/密码）、实时 Notify 计数、改阈值、清零 |
| 安全 | 配网阶段可绑定简易 PIN（可选） |

特征值建议：`telemetry`（Notify）、`wifi_cred`（Write）、`command`（Write）、`device_info`（Read）。

### 6.3 Wi-Fi MQTT

- 模式：STA；Broker 地址可 BLE 配置或固件默认（演示用公共/自建 EMQX）。  
- Topic：  
  - 上报 `device/{id}/telemetry`  
  - 下行 `device/{id}/cmd`  
- QoS：事件上报 QoS0/1 可配；心跳 30～60 s（仅 ACTIVE）。

### 6.4 4G MQTT（Cat.1 AT）

`lte_task` 状态机：

```
POWER_ON → PWRKEY → AT 就绪 → CPIN → CGATT
  → MQTTCONN → 订阅 cmd → 周期/事件 MQTTPUB
  → 失败重试 / 回退提示
```

载荷与 Wi-Fi **完全相同**。软件策略：

- `net_mode=wifi`：只跑 Wi-Fi；  
- `net_mode=lte`：只跑 4G；  
- `net_mode=auto`：Wi-Fi 可用优先 Wi-Fi，否则 4G；  
- **STANDBY 时不保持蜂窝连接**（断电）。

### 6.5 连接路由（`telemetry_service`）

```
publish(payload):
  if ble_connected: gatt_notify(payload)
  if net_mode 允许且 wifi_ready: mqtt_wifi_pub(payload)
  else if net_mode 允许且 lte_ready: mqtt_lte_pub(payload)
```

保证答辩可分别演示三通道，又避免无意义双活耗电。

---

## 7. 本地 UI 软件

### 7.1 OLED 页面

1. **主页**：IN / OUT / OCC  
2. **状态页**：电量、net（BLE/Wi-Fi/4G）、告警图标  
3. **设置摘要**：阈值、待机秒数（只读或按键进入简易设置）

刷新降频：无变化时降低刷屏，ACTIVE 有人时提高。

### 7.2 按键

| 操作 | 功能 |
|------|------|
| 短按 | 切页 |
| 长按 2 s | 进入 BLE 配网提示 |
| 长按 5 s | 清零日计数（可二次确认蜂鸣） |
| 双键组合 | 切换 `wifi` / `lte` 模式 |

### 7.3 蜂鸣器模式

- 超员：间歇长鸣直至 occ 下降或手动静音命令；  
- 低电：短鸣 3 声，间隔抑制，避免吵闹耗电。

---

## 8. 手机端 / 展示端软件（轻量必做）

为保证竞标与结项可演示，交付以下**轻量端**（不做重型原生 App）：

| 端 | 功能 | 实现方式 |
|----|------|----------|
| BLE 调试页 | 看实时 in/out/occ、写 Wi-Fi、下发阈值 | 微信小程序或基于 Web Bluetooth 的静态页（视平台支持） |
| MQTT 看板 | 曲线/当前人数/告警 | 单页 HTML + MQTT over WebSocket，或现成看板订阅同 topic |
| SoftAP 页（可选） | 无手机工具时应急看数 | 设备开热点 + 内置 HTTP 状态 JSON/简单 HTML |

**明确不做：** 人脸识别云平台、长时间录像回放、复杂权限中台。

---

## 9. 存储与配置

| 项 | 介质 | 说明 |
|----|------|------|
| Wi-Fi 凭证、Broker、阈值、T_idle、标定参数 | NVS | 掉电保存 |
| 日累计 in/out | NVS | 过线后异步写入；跨日可 RTC/软件日切清零 |
| 运行日志 | 可选环形 RAM / 串口 | 调试用；量产默认关 |

---

## 10. 目录与模块规划（固件仓库）

```
firmware/
  main/
    app_main.c
    tasks/          # tof/cam/count/ui/ble/wifi/lte/power/telemetry
    algo/           # cluster, track, line_cross, cam_count
    net/            # proto json, mqtt_route, ble_gatt
    power/          # standby, battery
    ui/             # oled, buzzer, keys
  components/
    vl53l8cx/       # ULD 移植
    cam_trigger/    # esp32-camera 封装
    lte_at/         # Cat.1 AT 状态机
    ssd1306/
```

语言：C（ESP-IDF 标准）。关键算法单元可用主机侧回放工具做离线调参（可选）。

---

## 11. 开发里程碑（软件）

| 阶段 | 交付 |
|------|------|
| S1 | 双 ToF 出数、串口打印深度与簇 |
| S2 | 多目标过线；OLED 显示 in/out/occ |
| S3 | 多人嫌疑触发相机点数；并排/尾随测试集 |
| S4 | BLE 配网 + Notify |
| S5 | Wi-Fi MQTT + 4G AT MQTT 统一载荷 |
| S6 | 分级待机 / 唤醒 / 低电；续航与准确率报告 |
| S7 | 小程序或网页看板、说明书、开源仓库整理 |

---

## 12. 测试用例（软件验收）

| 编号 | 用例 | 通过标准 |
|------|------|----------|
| T1 | 单人反复进出 100 次 | 综合准确率计入 ≥95% 样本 |
| T2 | 双人并行 50 次 | 同上 |
| T3 | 双人并行 + 一人随后 50 次 | 同上；允许跟距 ≥0.4 m |
| T4 | 超员 | occ 达阈值蜂鸣且 telemetry.alarm 置位 |
| T5 | 低电 | 模拟低压触发 UI/上报 |
| T6 | 无人 15 s | 进 STANDBY；电流下降；来人自动醒 |
| T7 | BLE | 配网成功、Notify 与本地一致 |
| T8 | Wi-Fi MQTT | 主题数据与本地一致 |
| T9 | 4G MQTT | 插卡注网后上报成功 |
| T10 | 模式切换 | wifi/lte 一键切换可演示 |

准确率统计口径：  
`准确率 = 1 - (|设备计数 - 人工真值| / 人工真值)`，分场景加权或综合报表。

---

## 13. 风险与软件对策

| 风险 | 对策 |
|------|------|
| ToF 贴身并排粘连 | 宽度启发式 + 触发灰度 `person_hint` |
| 尾随漏计 | 多 track + 短时保持；禁止整帧单次计数 |
| MQTT 双通道冲突 | 路由层互斥；STANDBY 全断 |
| Light Sleep 唤醒误触发 | 醒来后 1～2 s 无目标则再睡 |
| 相机驱动占内存 | 仅 QQVGA；用完释放；依赖 N8R8 PSRAM |
| AT 模组差异 | `lte_at` 适配层隔离 Air780E/EC800E 指令差异 |

---

## 14. 开源与交付物清单

1. 完整 ESP-IDF 工程（可编译固件）  
2. 协议说明（JSON 字段、Topic、GATT UUID 表）  
3. 手机端/网页最小演示工程  
4. 标定与测试报告模板（含准确率表）  
5. 串口日志说明与常见问题  

与硬件技术方案书配套提交；硬件定型主控为 **ESP32-S3-WROOM-1-N8R8**。

---

## 15. 总结

本软件方案在 **ESP32-S3-WROOM-1-N8R8** 上实现：双 ToF 多目标双向计数、触发式灰度人数校验、OLED/蜂鸣本地交互、BLE + Wi-Fi + 4G 统一遥测、以及无人分级低功耗与低电告警。算法与通信路径均为可落地实现，覆盖悬赏对准确率、双向统计、多模无线与低功耗的软件要求，适合作为星火计划软件侧竞标与结项依据。

---

**文档结束**
