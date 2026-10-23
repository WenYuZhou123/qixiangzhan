# qixiangzhan 气象站边缘主站

这是一个面向现场落地的气象站与远程运维平台工程，核心形态是：

- `STM32H743 + L610 + LCD` 负责本地采集、显示、4G/MQTT 上报和现场控制。
- `Jetson + FastAPI + MySQL + MQTT bridge` 负责边缘主站、数据入库、健康监控和本地服务。
- `Qt Desktop + Qt Android` 负责桌面值守、移动端查看、命令下发、告警和历史数据。
- `Cloudflare Tunnel` 负责公网 HTTPS 访问，不再依赖腾讯云 80/443 入口。

当前正式 API 地址：

```text
https://qixiangzhan.online/api/v1
```

健康检查地址：

```text
https://qixiangzhan.online/healthz
```

## 系统架构

```text
STM32H743 + Sensors + LCD
        |
        | L610 / MQTT TLS
        v
      EMQX
        |
        v
Jetson edge master
  - FastAPI
  - MySQL
  - MQTT bridge
  - system health
        |
        | Cloudflare Tunnel / HTTPS / WebSocket
        v
Qt Desktop / Qt Android / Browser docs
```

## 仓库目录

```text
qixiangzhan/
├─ Core/                 STM32CubeMX 生成代码，谨慎改动路径
├─ Drivers/              STM32 HAL / CMSIS，工具链依赖目录
├─ MDK-ARM/              Keil 工程文件和启动文件
├─ HARDWARE/             L610、LCD、传感器、业务驱动代码
├─ backend/              FastAPI 后端、MySQL 模型、MQTT bridge
├─ qt-client/            Qt 桌面端和 Android App
├─ deploy/linux/         Jetson/Linux 部署样例、systemd、Caddy、备份脚本
├─ docs/                 部署、联调、数据库、验收文档
├─ scripts/              Windows 构建、启动、Qt/Android 辅助脚本
└─ qixiangzhan.ioc       STM32CubeMX 工程入口
```

更详细的代码地图见 [docs/repository_map_zh.md](docs/repository_map_zh.md)。

## 快速启动

### 1. 启动后端

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\backend
py -3.14 -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
Copy-Item .env.example .env
alembic -c alembic.ini upgrade head
py -3.14 -m uvicorn app.main:app --reload
```

本地后端默认监听：

```text
http://127.0.0.1:8000
```

### 2. 启动 Qt 桌面端

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\start_desktop_stack.cmd
```

桌面端和 Android App 的生产 API 默认值已经统一为：

```text
https://qixiangzhan.online/api/v1
```

### 3. 构建 Android APK

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\build_android_apk.cmd
```

Release 包可通过环境变量覆盖 API：

```powershell
$env:QXZ_CLIENT_DEFAULT_API_BASE = "https://qixiangzhan.online/api/v1"
.\scripts\build_android_apk.cmd -Configuration Release
```

## 交付验收路径

### Jetson / 后端

- `GET https://qixiangzhan.online/healthz` 返回 `status=ok`。
- `POST /api/v1/auth/login` 可登录。
- `GET /api/v1/devices` 能返回设备列表。
- `GET /api/v1/system/health` 能看到 API、MySQL、MQTT、磁盘和最近设备数据。
- MQTT 上报后，`devices`、`telemetry_messages`、`weather_observations`、`weather.project` 数据持续增长。

### Qt 桌面端

- 使用 `https://qixiangzhan.online/api/v1` 登录。
- 能查看设备在线状态、核心气象数据、告警、命令历史和 Jetson 健康摘要。
- 工程模式下可使用串口诊断和本地调试功能。

### Android App

- 使用公网 HTTPS API 登录，不直接连接 MQTT 或 MySQL。
- 设备列表、实时状态、历史、告警和控制命令可用。
- 弱网或 WebSocket 不可用时，REST 轮询仍能保持基本可用。

### STM32 / 现场端

- LCD 页面无明显整屏闪烁。
- L610 MQTT 链路能稳定连接 EMQX。
- 风速/风向、雨滴、CJ702 空气质量等传感器状态能上报诊断信息。
- 控制命令有 ACK 闭环。

## 关键文档

- 文档导航：[docs/README.md](docs/README.md)
- Jetson 现场部署：[docs/jetson_edge_master_guide_zh.md](docs/jetson_edge_master_guide_zh.md)
- Qt/Android 客户端：[qt-client/README.md](qt-client/README.md)
- 后端服务：[backend/README.md](backend/README.md)
- Linux 部署包：[deploy/linux/README.md](deploy/linux/README.md)
- Navicat/MySQL：[docs/navicat_mysql_setup.md](docs/navicat_mysql_setup.md)
- 硬件联调：[docs/h743_l610_qt_quick_start.md](docs/h743_l610_qt_quick_start.md)

## Git 提交建议

建议提交：

- `HARDWARE/`、`Core/`、`Drivers/`、`MDK-ARM/` 中的固件源码和工程定义。
- `backend/` 后端源码、迁移脚本和依赖清单。
- `qt-client/` 客户端源码、QML、CMake 配置。
- `docs/`、`deploy/`、`scripts/`。
- `README.md`、`.gitignore`、`.gitattributes`、`.editorconfig`。

不要提交：

- Python 虚拟环境、`.env`、密钥、证书、Android keystore。
- Qt/Android 构建目录、APK/AAB、本地 DLL/EXE 输出。
- Keil 中间产物和输出文件。
- 本地数据库、日志、临时文件。

本仓库保留 STM32CubeMX/Keil 的原始目录布局。除非同步更新工具链配置，不要移动 `Core/`、`Drivers/`、`MDK-ARM/` 或 `qixiangzhan.ioc`。
- 固件源码与头文件
- `backend/` 后端源码
- `qt-client/` 客户端源码
- `docs/`
- `deploy/linux/`
- `scripts/`
- Keil 工程定义文件：`*.uvprojx`、`*.sct`、`RTE_Components.h`、`*.dbgconf`

不建议提交：

- Python 虚拟环境
- Qt / Android 构建目录
- Keil 中间产物与输出文件：`.crf`、`.d`、`.dep`、`.map`、`.htm`、`.hex`、`.lnp`
- 本机 Keil 配置文件：`MDK-ARM/qixiangzhan.uvoptx`
- 本地数据库、日志、缓存
- keystore、签名文件、`.env`
- 临时 PDF、调试草稿和本地手册文件

行尾与文本文件约定：

- 源码、文档和普通配置文本默认使用 LF
- Windows 启动脚本与 PowerShell 脚本保留 CRLF
- 重新打开 Keil 工程后，如果只出现 `uvoptx`、`map`、`hex`、构建日志等变化，默认不要提交

## Qt 客户端启动脚本

以下脚本按当前本机目录约定编写，直接在 PowerShell 中执行即可。

### 启动 Qt 桌面端

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
$exe = "D:\Competition\code_main\qixiangzhan\qt-client\build\desktop-mingw\relay_qt_client.exe"
Get-Process relay_qt_client -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Process -FilePath $exe
```

### 使用雷电模拟器启动 Android App

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
$ldAdb = "D:\leidian\LDPlayer9\adb.exe"
$activity = "com.qixiangzhan.platform/org.qtproject.qt.android.bindings.QtActivity"
& $ldAdb devices
& $ldAdb -s emulator-5554 shell am start -W -n $activity
```

### 重新安装当前 APK 到雷电模拟器

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
$ldAdb = "D:\leidian\LDPlayer9\adb.exe"
$apk = "D:\Competition\code_main\qixiangzhan\qixiangzhan-android-debug.apk"
& $ldAdb -s emulator-5554 install -r $apk
& $ldAdb -s emulator-5554 shell am start -W -n com.qixiangzhan.platform/org.qtproject.qt.android.bindings.QtActivity
```
