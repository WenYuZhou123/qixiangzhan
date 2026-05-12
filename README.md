# qixiangzhan 边缘主站项目

这是一个面向现场落地的气象站边缘主站工程，当前形态是：

- `STM32H743 + L610 + LCD` 负责本地采集、显示、MQTT 上报和现场控制
- `Jetson + FastAPI + MySQL + MQTT bridge` 负责边缘主站、数据入库、健康监控和本地服务
- `Qt Desktop + Qt Android` 负责运维值守、状态查看、命令下发、告警和历史查询
- `Cloudflare Tunnel` 负责公网 HTTPS 访问

正式 API：

```text
https://qixiangzhan.online/api/v1
```

健康检查：

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
├─ Drivers/              STM32 HAL / CMSIS
├─ MDK-ARM/              Keil 工程文件
├─ HARDWARE/             L610、LCD、传感器和业务驱动
├─ backend/              FastAPI 后端、MySQL 模型、MQTT bridge
├─ qt-client/            Qt 桌面端和 Android App
├─ deploy/linux/         Jetson/Linux 部署包、systemd、Caddy、备份脚本
├─ docs/                 部署、联调、数据库和验收文档
├─ scripts/              Windows 构建与启动脚本
└─ qixiangzhan.ioc       STM32CubeMX 工程入口
```

代码地图见 [docs/repository_map_zh.md](docs/repository_map_zh.md)。

## 快速启动

### 后端

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\backend
py -3.14 -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
Copy-Item .env.example .env
alembic -c alembic.ini upgrade head
py -3.14 -m uvicorn app.main:app --reload
```

### Qt 桌面端

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\start_desktop_stack.cmd
```

### Android APK

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\build_android_apk.cmd
```

## 当前交付状态

已打通并验收过的链路：

- Jetson 本机 FastAPI 正常
- MySQL 主库 `qixiangzhan` 正常
- Navicat 兼容库 `weather.project` 正常
- MQTT bridge 已连接
- Cloudflare Tunnel 公网入口正常
- `qixiangzhan.online` 与 `www.qixiangzhan.online` 均可访问
- `admin / admin123` 登录正常
- `/api/v1/system/health`、`/devices`、`/project/latest` 正常

## 关键文档

- [backend/README.md](backend/README.md)
- [deploy/linux/README.md](deploy/linux/README.md)
- [qt-client/README.md](qt-client/README.md)
- [docs/README.md](docs/README.md)
- [docs/jetson_edge_master_guide_zh.md](docs/jetson_edge_master_guide_zh.md)
- [docs/public_remote_deployment_guide.md](docs/public_remote_deployment_guide.md)

## Git 提交建议

建议提交：

- `backend/`
- `qt-client/`
- `deploy/`
- `docs/`
- `README.md`

不要提交：

- `.env`
- 私钥、token、证书、keystore
- Python 虚拟环境
- Qt / Android 构建产物
- Keil 中间产物
- 日志、数据库和临时文件
