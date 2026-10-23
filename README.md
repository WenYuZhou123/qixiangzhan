# 气象站边缘主站

面向无人机停机坪现场的气象采集、远程控制和运维平台。系统由 STM32 现场控制器、ESP32-S3 本地触摸面板、L610 MQTT 链路、FastAPI 边缘服务，以及 Qt 桌面端/Android 客户端组成。

## 系统架构

```text
STM32H743 + sensors + LCD <---- RS485 ----> ESP32-S3 touch panel
          |
          | L610 / MQTT TLS
          v
        EMQX
          |
          v
FastAPI edge service + database + MQTT bridge
          |                         |
          | HTTPS / WebSocket       | OpenAI-compatible provider
          v                         v
Qt Desktop / Android             AI analysis and forecast
```

主要能力：

- STM32H743 采集风速风向、雨量、空气质量等传感器数据，并执行继电器和停机坪控制命令。
- L610 通过 MQTT TLS 上报状态、接收命令并完成 ACK 闭环。
- ESP32-S3 本地面板显示实时状态、气象、停机坪和系统诊断，可配置 Wi-Fi 与边缘 AI 服务。
- 后端提供鉴权、设备状态、历史记录、告警、MQTT 桥接、系统健康检查、AI 分析与天气预报接口。
- Qt 桌面端和 Android 客户端通过 HTTPS/WebSocket 进行远程值守、查看告警和下发控制命令。
- 可选 WebRTC 图传部署由 MediaMTX、coturn 和 Jetson 视频转发组成。

## 目录

```text
qixiangzhan/
├─ Core/                 STM32CubeMX 生成代码
├─ HARDWARE/             传感器、L610、RS485、协议和业务驱动
├─ Drivers/              STM32 HAL / CMSIS
├─ MDK-ARM/              Keil 工程与启动文件
├─ esp32-local-panel/    ESP32-S3-Touch-LCD-7 本地面板工程
├─ backend/              FastAPI、数据模型、MQTT bridge、AI/预报服务
├─ qt-client/            Qt 桌面端与 Android 客户端
├─ deploy/linux/         Linux/Jetson 后端部署样例
├─ deploy/video/         MediaMTX、coturn、Jetson 视频转发部署样例
├─ docs/                 平台、数据库和联调文档
└─ scripts/              构建与启动辅助脚本
```

## 本地启动

### 后端

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\backend
py -3.14 -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
Copy-Item .env.example .env
py -3.14 -m uvicorn app.main:app --reload
```

本地健康检查：`http://127.0.0.1:8000/healthz`。

生产环境应使用 `.env.example` 作为模板，配置数据库、MQTT、令牌密钥和边缘服务密钥；不要提交 `.env`。

### Qt 桌面端与 Android

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\start_desktop_stack.cmd
.\scripts\build_android_apk.cmd
```

客户端通过 HTTPS API 工作，不直接连接 MQTT 或数据库。API 地址可由客户端设置页或构建环境变量覆盖。

### ESP32-S3 本地面板

使用 ESP-IDF 6.0.2：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\esp32-local-panel
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

在 `menuconfig -> Cloud AI` 设置初始 Wi-Fi、边缘 API URL、边缘密钥和设备 ID。设备屏幕的“设置”页也可以修改 Wi-Fi 与边缘服务，保存后写入 NVS 并覆盖编译默认值。

`sdkconfig` 与 `sdkconfig.old` 是本机配置，已被 Git 忽略。提交 `sdkconfig.defaults` 时必须保持 Wi-Fi 密码和 API Key 为空。

推荐生产模式是让 ESP32 请求后端的：

```text
POST /api/v1/edge/ai/analyze
```

此模式使用 `X-QXZ-Edge-Key`，后端再调用云模型。直连 OpenAI 兼容接口仅适合受限额度的专用密钥和测试环境。

## 远程服务与验收

- `GET /healthz`：后端存活检查。
- `GET /api/v1/system/health`：鉴权后的 API、数据库、MQTT、磁盘与设备摘要。
- `POST /api/v1/edge/ai/analyze`：ESP32 边缘 AI 分析接口。
- `POST /api/v1/edge/weather/forecast`：ESP32 气象预报接口。
- `WS /api/v1/ws/realtime`：客户端实时状态推送。

ESP32 边缘接口不使用用户 JWT，而是使用独立的设备密钥。用户端接口除登录、刷新和退出外，均需要 Bearer Token。

## 视频链路

WebRTC 图传是可选模块。部署 MediaMTX/coturn 与 Jetson 视频转发前，请阅读 [视频部署说明](deploy/video/README.md)。视频域名不能经过 Cloudflare HTTP 代理，且需要按该文档开放 WebRTC、TURN 和 RTSP 端口。

## 文档入口

- [后端说明](backend/README.md)
- [ESP32 本地面板说明](esp32-local-panel/README.md)
- [Qt 客户端说明](qt-client/README.md)
- [Linux/Jetson 部署说明](deploy/linux/README.md)
- [视频部署说明](deploy/video/README.md)
- [项目文档目录](docs/README.md)

## 版本控制与密钥

提交源码、构建配置模板和部署样例；不要提交 `.env`、Wi-Fi 密码、API Key、证书、Android keystore、构建输出、数据库、日志或本机 Keil 配置。STM32CubeMX/Keil 目录结构与 `qixiangzhan.ioc` 彼此关联，除非同步修改工具链配置，不要移动 `Core/`、`Drivers/` 或 `MDK-ARM/`。
