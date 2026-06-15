# 智能安防监控系统 Smart Surveillance System

基于 ESP32-S3 / ESP32-CAM / STM32F103 的三端智能安防系统，具备 AI 火焰检测、实时视频流、环境传感器监控和 Web 仪表盘功能。

## 系统架构

```
┌─────────────┐     UART (115200)      ┌────────────────┐     HTTP / WiFi      ┌──────────────┐
│  STM32F103  │ ◄──────────────────► │    ESP32-S3    │ ◄─────────────────► │  ESP32-CAM   │
│  传感器端    │  T/H/L/Beep 数据帧    │   中央处理端    │   JPEG 帧抓取        │  摄像头端     │
│             │  Beeper 控制指令       │               │                     │              │
│ DHT11 温湿度 │                        │ AI 火焰检测     │                     │ MJPEG 视频流  │
│ BH1750 光照  │                        │ Web 仪表盘     │                     │ HTTP 拍照     │
│ OLED 显示屏  │                        │ WebSocket 推送 │                     │              │
│ 蜂鸣器 报警  │                        └──────┬─────────┘                     └──────────────┘
└─────────────┘                               │
                                              │ WebSocket + HTTP
                                              ▼
                                        ┌──────────┐
                                        │  浏览器    │
                                        │  仪表盘    │
                                        └──────────┘
```

## 硬件清单

| 开发板 | 型号 | 功能 |
|--------|------|------|
| 主控 | ESP32-S3 | AI 推理、Web 服务器、多端协调 |
| 摄像头 | ESP32-CAM (AI-Thinker) | JPEG 抓拍、MJPEG 视频流 |
| 传感器 | STM32F103C8T6 | 环境数据采集、显示、报警 |

### 传感器与外设 (STM32)

| 器件 | 接口 | 功能 |
|------|------|------|
| DHT11 | One-wire (PA6) | 温度 + 湿度 |
| BH1750 | I2C2 (PB10/PB11) | 环境光照度 |
| MPU6050 | I2C (驱动已集成) | 6 轴姿态（预留） |
| OLED 128×64 | I2C (PB8/PB9) | 四行数据显示 |
| 有源蜂鸣器 | GPIO (PB12) | 异常报警 |
| LED ×2 | GPIO (PA1/PA2) | 状态指示 |

## 功能特性

- **AI 火焰检测** — MobileNetV2 INT8 量化模型在 ESP32-S3 上运行，结合温度自适应阈值（高温降低阈值提升灵敏度，低温提高阈值减少误报）
- **实时视频流** — ESP32-CAM 提供 MJPEG 流，浏览器直接观看
- **环境监控** — STM32 每 0.5 秒上报温湿度、光照数据，ESP32-S3 通过 WebSocket 推送至浏览器
- **综合报警** — 火焰检测 / 光照 > 3000 lux / 湿度 > 80% 任一触发蜂鸣器
- **Web 仪表盘** — 单页应用，含传感器卡片、火焰检测动画、视频嵌入、调试日志

## 目录结构

```
smart_surveillance/
├── esp32-s3/              # ESP32-S3 中央处理端 (ESP-IDF)
│   ├── main/
│   │   ├── smart_surveillance.c   # 主程序入口
│   │   ├── ai_detection/          # AI 火焰检测 & 图像处理
│   │   │   ├── fire_detector.cpp  # MobileNetV2 推理
│   │   │   ├── image_processing.c # JPEG 解码 & 检测流程
│   │   │   ├── get_image.c        # HTTP 抓取 CAM 图像
│   │   │   └── models/            # TFLite 模型文件
│   │   ├── iot/                   # WiFi & WebSocket 服务
│   │   ├── hal/                   # UART 驱动
│   │   └── common/                # 共享数据结构
│   └── components/                # ESP 组件 (esp-tflite-micro 等)
├── esp32-cam/              # ESP32-CAM 摄像头端 (ESP-IDF)
│   └── main/
│       ├── app_camera.c           # 摄像头初始化
│       ├── app_httpd.c            # HTTP + MJPEG 服务器
│       ├── app_wifi.c             # WiFi 管理
│       └── www/                   # 内嵌网页
└── stm32/                  # STM32F103 传感器端 (Keil MDK)
    ├── User/main.c                # 主程序
    ├── Hardware/                  # 硬件驱动
    │   ├── my_dht11.c             # DHT11 温湿度
    │   ├── bh1750.c               # BH1750 光照
    │   ├── MPU6050.c              # MPU6050 IMU（预留）
    │   ├── OLED.c                 # OLED 显示
    │   ├── beeper.c               # 蜂鸣器
    │   └── Serial.c               # UART 通信
    ├── System/                    # 系统定时器 & 延时
    └── Timer.c                    # 定时发送控制
```

## 数据协议

### UART 通信 (STM32 → ESP32-S3)

```
T{温度}H{湿度}L{光照}S{蜂鸣器状态}#
```
示例: `T25H60L1500S0#` 表示温度 25°C，湿度 60%，光照 1500 lux，蜂鸣器关闭。

### UART 控制指令 (ESP32-S3 → STM32)

| 指令 | 含义 |
|------|------|
| `0x01` | 打开蜂鸣器 |
| `0x00` | 关闭蜂鸣器 |

### WebSocket 推送 JSON (ESP32-S3 → 浏览器)

```json
// 传感器数据
{"temperature": 25, "humidity": 60, "light_intensity": 1500, "beep_status": 0}

// 火焰检测告警
{"type": "fire_alert", "detected": true, "confidence": 0.85}
```

## 编译与烧录

> **重要：** 两个 ESP32 端使用的 IDF 版本不同，请严格按照版本要求编译。

### ESP32-S3（中央处理端）

- **IDF 版本：** v5.0 及以上

```bash
cd esp32-s3
idf.py set-target esp32s3
idf.py menuconfig   # 配置 WiFi SSID/密码、PSRAM 等
idf.py build flash monitor
```

### ESP32-CAM（摄像头端）

- **IDF 版本：** v4.4（必须使用此版本，高版本不兼容）

```bash
cd esp32-cam
idf.py set-target esp32
idf.py menuconfig   # 配置摄像头引脚（默认 AI-Thinker）、WiFi
idf.py build flash monitor
```

### STM32F103（传感器端）
使用 Keil MDK 打开 `stm32/Project.uvprojx`，编译后通过 ST-Link 或串口烧录。

## 使用说明

1. 先给 ESP32-CAM 上电，确认摄像头正常工作
2. 给 STM32 上电，OLED 显示传感器数据
3. 给 ESP32-S3 上电，自动连接 WiFi 并建立通信
4. 浏览器访问 ESP32-S3 的 IP 地址（默认端口 80），查看仪表盘
5. 异常情况蜂鸣器自动报警，仪表盘同步告警动画

## 技术栈

- **AI 推理** — TensorFlow Lite Micro + MobileNetV2 INT8 + ESP NN 加速
- **图像处理** — ESP 硬件 JPEG 解码器 (`esp_new_jpeg`)
- **通信** — WebSocket + HTTP + UART
- **前端** — 原生 HTML/CSS/JS（内嵌于固件）
- **RTOS** — FreeRTOS 双核任务调度

## 许可证

MIT License
