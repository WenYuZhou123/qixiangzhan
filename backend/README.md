# qixiangzhan backend

FastAPI 后端负责用户鉴权、设备状态聚合、MQTT bridge、MySQL 入库、WebSocket 推送、系统健康检查和 Navicat `weather.project` 兼容写入。

生产 API：

```text
https://qixiangzhan.online/api/v1
```

本地健康检查：

```text
http://127.0.0.1:8000/healthz
```

## 启动

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\backend
py -3.14 -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
Copy-Item .env.example .env
alembic -c alembic.ini upgrade head
py -3.14 -m uvicorn app.main:app --reload
```

## 关键配置

主业务数据库：

```text
QXZ_DATABASE_URL=mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/qixiangzhan?charset=utf8mb4
```

可选 Navicat `weather.project` 兼容库：

```text
QXZ_PROJECT_DATABASE_URL=mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/weather?charset=utf8mb4
```

公网 API 基准地址：

```text
QXZ_PUBLIC_API_BASE=https://qixiangzhan.online/api/v1
```

## 数据链路

- 原始 MQTT 报文写入 `telemetry_messages`。
- 最新设备状态写入 `devices` 和 `device_last_state`。
- 结构化气象数据写入 `weather_observations`。
- Navicat 新表格式双写到 `weather.project`，失败时只记录 `system_events`，不影响主链路。
- 命令下发和 ACK 记录写入 `command_messages`。
- 设备离线、传感器离线、MQTT/数据库/磁盘异常写入 `alarms` 或 `system_events`。

## API 摘要

- `POST /api/v1/auth/login`
- `POST /api/v1/auth/refresh`
- `POST /api/v1/auth/logout`
- `GET /api/v1/auth/me`
- `GET /api/v1/devices`
- `GET /api/v1/devices/{device_id}`
- `GET /api/v1/devices/{device_id}/history`
- `GET /api/v1/devices/{device_id}/weather/history`
- `GET /api/v1/devices/{device_id}/diagnostics`
- `GET /api/v1/project/latest`
- `GET /api/v1/project/{device_id}/history`
- `GET /api/v1/devices/{device_id}/alarms`
- `GET /api/v1/commands`
- `POST /api/v1/devices/{device_id}/commands`
- `GET /api/v1/system/health`
- `WS /api/v1/ws/realtime`
- `GET /healthz`

除登录、刷新和退出外，`/api/v1/*` 接口都需要：

```text
Authorization: Bearer <access_token>
```

## 运维验证

```powershell
curl.exe https://qixiangzhan.online/healthz
curl.exe https://qixiangzhan.online/openapi.json
```

本地验证：

```powershell
curl.exe http://127.0.0.1:8000/healthz
```

更多说明：

- [平台运维手册](../docs/platform_operation_manual_zh.md)
- [Jetson 现场部署手册](../docs/jetson_edge_master_guide_zh.md)
- [Navicat/MySQL 配置](../docs/navicat_mysql_setup.md)
