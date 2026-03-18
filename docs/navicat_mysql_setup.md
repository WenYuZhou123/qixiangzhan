# Navicat + MySQL Setup

## 1. Create database

In Navicat or MySQL shell, create the database:

```sql
CREATE DATABASE qixiangzhan
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;
```

## 2. Create app user

```sql
CREATE USER 'qixiang_app'@'%' IDENTIFIED BY 'change-me';
GRANT ALL PRIVILEGES ON qixiangzhan.* TO 'qixiang_app'@'%';
FLUSH PRIVILEGES;
```

If the backend runs on the same Windows machine only, you can replace `'%'` with `'localhost'`.

## 3. Configure backend

Set `backend/.env`:

```text
QXZ_DATABASE_URL=mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/qixiangzhan?charset=utf8mb4
```

Then start the backend:

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\backend
.venv\Scripts\python.exe -m uvicorn app.main:app --reload
```

The backend will auto-create and auto-complete tables.

## 4. Verify in Navicat

After first startup, confirm these tables exist:

- `users`
- `devices`
- `device_last_state`
- `telemetry_messages`
- `command_messages`
- `alarms`

## 5. Common checks

Recent device state:

```sql
SELECT device_id, display_name, online, relay1, relay2, rssi, operator_name, ip, updated_at
FROM devices
ORDER BY updated_at DESC;
```

Recent command records:

```sql
SELECT msg_id, device_id, command, value, status, detail, created_at, acked_at
FROM command_messages
ORDER BY created_at DESC
LIMIT 50;
```

Active alarms:

```sql
SELECT device_id, code, severity, message, source, created_at
FROM alarms
WHERE active = 1
ORDER BY created_at DESC;
```

Raw telemetry:

```sql
SELECT device_id, topic, direction, event_type, payload, created_at
FROM telemetry_messages
ORDER BY created_at DESC
LIMIT 100;
```

## 6. What data should grow

When the board is online and publishing:

- `devices` should update the latest relay/network state.
- `device_last_state` should keep the latest normalized payload.
- `telemetry_messages` should keep growing with status, ack, online and event messages.
- `command_messages` should grow when desktop or Android sends commands.
- `alarms` should change when offline, ACK timeout or low RSSI happens.

## 7. Notes

- Navicat is only used for management and verification; the backend remains the source of schema creation.
- All timestamps are written by the application in UTC.
- Legacy topics such as `device/relay/status` and escaped JSON payloads are supported by the backend.
