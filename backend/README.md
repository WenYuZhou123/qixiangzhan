# qixiangzhan backend

FastAPI + MySQL + EMQX bridge for the Qt desktop and Android client.

Full manual: `..\docs\platform_operation_manual_zh.md`
Public remote deployment: `..\docs\public_remote_deployment_guide.md`

## Run

```powershell
py -3.14 -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
Copy-Item .env.example .env
alembic -c alembic.ini upgrade head
py -3.14 -m uvicorn app.main:app --reload
```

Default database URL:

```text
mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/qixiangzhan?charset=utf8mb4
```

Optional Navicat `weather.project` mirror database URL:

```text
mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/weather?charset=utf8mb4
```

Recommended production public API base:

```text
https://api.qixiangzhan.online/api/v1
```

## Navicat / MySQL

- Navicat guide: `..\docs\navicat_mysql_setup.md`
- Create database: `qixiangzhan`
- Create app user: `qixiang_app`
- Start backend once, then verify tables:
  - `users`
- `devices`
- `device_last_state`
- `telemetry_messages`
- `weather_observations`
- `command_messages`
- `alarms`
- `system_events`

## Desktop and Android

- Hardware guide: `..\docs\h743_l610_qt_quick_start.md`
- Desktop launcher: `..\scripts\start_desktop_stack.cmd`
- Android build: `..\scripts\build_android_apk.cmd`

## APIs

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
- `GET /api/v1/users`
- `POST /api/v1/users`
- `PATCH /api/v1/users/{user_id}`
- `POST /api/v1/users/{user_id}/devices`
- `WS /api/v1/ws/realtime`
- `GET /api/v1/system/health`
- `GET /healthz`

All `/api/v1/*` endpoints except login/refresh/logout require `Authorization: Bearer <token>`.

## MQTT bridge

The backend subscribes to:

- `device/+/down/cmd`
- `device/+/up/status`
- `device/+/up/ack`
- `device/+/up/event`
- `device/+/up/online`
- `device/+/status`

The backend also normalizes:

- legacy topic `device/<legacy_key>/status`
- escaped JSON payloads
- `client_id -> device_id`
- `net.rssi / net.operator / net.ip`

## Roles

- `admin`: manage users, assign devices, full device access
- `operator`: access only assigned devices
