# STM32H743 + L610 Qt Platform Quick Start

This guide is for the current `STM32H743 + L610 + EMQX` relay project in this repo.

## 1. Default project parameters

The current firmware, Qt client, and backend already use the same defaults:

- Device ID: `relay_h743_001`
- MQTT host: `rc11adc1.ala.cn-hangzhou.emqxsl.cn`
- MQTT port: `8883`
- MQTT username: `h743`
- MQTT password: `123456`
- MQTT TLS: `enabled`
- API base for local lab bring-up: `http://127.0.0.1:8000/api/v1`
- API base for public deployment: `https://api.qixiangzhan.online/api/v1`
- Backend login: `admin / admin123`
- Serial baud rate: `115200`

MQTT topics:

- `device/relay_h743_001/down/cmd`
- `device/relay_h743_001/up/status`
- `device/relay_h743_001/up/ack`
- `device/relay_h743_001/up/event`
- `device/relay_h743_001/up/online`

Supported Qt command set:

- `set_r1`
- `set_r2`
- `set_all`
- `query_status`

Firmware serial presets:

- `ATRAW`
- `SELFTEST`
- `NETINIT`
- `MQTTINIT`
- `MQTTCLOSE`
- `STATUS`

## 2. Start the desktop stack

Recommended one-click entry:

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
powershell -ExecutionPolicy Bypass -File .\scripts\start_desktop_stack.ps1
```

Or use the wrapper:

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\start_desktop_stack.cmd
```

If you are not in the repo root, use the absolute script path:

```powershell
powershell -ExecutionPolicy Bypass -File "D:\Competition\code_main\qixiangzhan\scripts\start_desktop_stack.ps1"
```

The script will:

- start the FastAPI backend with `uvicorn`
- deploy the Qt desktop runtime automatically if DLLs are missing
- launch the built Qt desktop client

If you only want to repair the desktop runtime package:

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\deploy_desktop_qt.cmd
```

Current desktop client executable:

- `qt-client\build\desktop-verify\relay_qt_client.exe`

## 2.1 Weather sensor wiring note

Before testing the weather module on hardware, use the following conservative wiring rules:

- `CJ702-U` air module:
  - `VDD` -> external sensor supply
  - `GND` -> external sensor ground
  - `GND` -> STM32 GND
  - `TX` -> `USART3_RX (PB11)`
  - `NC` left floating
- If the `CJ702-U` `TX` output is 5V TTL, do not connect it directly to `PB11`.
  Add level shifting or a safe divider first.
- `ZTS-3000-FSJT-V02` wind-speed transmitter:
  - brown -> sensor supply positive (`10~30V`)
  - black -> sensor supply negative
  - blue -> output positive
  - yellow/green -> output negative
- Do not directly connect the raw wind transmitter output pair to `PA6/PC4`.
  The current firmware only supports testing after a differential-to-single-ended
  analog frontend has converted the sensor output into an MCU-safe ADC voltage.

If you want to start pieces manually:

```powershell
cd backend
.venv\Scripts\python.exe -m uvicorn app.main:app --host 127.0.0.1 --port 8000 --reload
```

Then open:

```powershell
qt-client\build\desktop-verify\relay_qt_client.exe
```

## 3. Desktop login flow

After opening the Qt desktop app:

1. In `登录页`, keep `API Base` as `http://127.0.0.1:8000/api/v1`.
2. Use `admin / admin123` to sign in.
3. If the backend is not running, you can still use the local fallback account `admin / admin123`.

For public deployment across different LANs, switch `API Base` to `https://api.qixiangzhan.online/api/v1`.

Main pages in the current app:

- `登录`
- `设备总览`
- `历史`
- `告警`
- `设置`
- `串口诊断`
- `运行日志`

## 4. MQTT connection setup

Go to `设置` and fill the current project values:

- Host: `rc11adc1.ala.cn-hangzhou.emqxsl.cn`
- Port: `8883`
- Username: `h743`
- Password: `123456`
- Current Device: `relay_h743_001`
- TLS: `开启`

Then:

1. Click `连接`
2. Wait for the status to become connected
3. Click `查询当前设备`
4. Open `运行日志` to confirm subscribe and message flow

If the board is online, you should receive:

- online state
- status report
- ACK messages after commands

## 5. Relay control workflow

Open `设备总览`, select `relay_h743_001`, and enter the detail panel.

Available actions in the current Qt client:

- `R1 ON`
- `R1 OFF`
- `R2 ON`
- `R2 OFF`
- `全部打开`
- `全部关闭`
- `查询当前设备`
- `重试上次命令`

Recommended first validation sequence:

1. Click `查询当前设备`
2. Confirm relay state, RSSI, IP, and last update time appear
3. Click `R1 ON`
4. Watch `设备详情`, `历史`, and `运行日志`
5. Confirm an ACK is returned and the status refreshes
6. Repeat with `R1 OFF`, `R2 ON`, `R2 OFF`

## 6. Serial console workflow

If you want to debug the board locally through UART:

1. Open `串口诊断`
2. Click `刷新串口`
3. Select the board COM port
4. Keep baud rate at `115200`
5. Click `连接`

Recommended serial sequence for first bring-up:

1. `SELFTEST`
2. `NETINIT`
3. `MQTTINIT`
4. `STATUS`

What each preset is for:

- `SELFTEST`: basic board and module self-check
- `NETINIT`: cellular network init
- `MQTTINIT`: MQTT login after network is ready
- `STATUS`: current relay, network, and MQTT summary
- `MQTTCLOSE`: close MQTT session cleanly
- `ATRAW`: send raw AT commands to the L610 module

You can also send direct relay commands from the serial page:

- `R1_ON`
- `R1_OFF`
- `R2_ON`
- `R2_OFF`
- `ALL_ON`
- `ALL_OFF`
- `STATUS`

## 7. Backend and database notes

The backend defaults are read from `backend\.env` or `backend\.env.example`.

Current default values:

- PostgreSQL: `postgresql+psycopg://postgres:postgres@localhost:5432/qixiangzhan`
- Offline timeout: `90` seconds
- ACK timeout: `15` seconds

Minimum backend tables already expected by the system:

- `users`
- `devices`
- `device_last_state`
- `telemetry_messages`
- `command_messages`
- `alarms`

If login works but history is empty, check:

- PostgreSQL service is running
- the database `qixiangzhan` exists
- backend logs show successful startup
- EMQX credentials match the firmware and Qt client

## 8. Android use

Build the Android package with:

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\build_android_apk.cmd
```

Current APK output:

- `qt-client\build\android-arm64\android-build\build\outputs\apk\debug\android-build-debug.apk`

The Android app reuses the same MQTT host, device ID, login flow, and data model.
The main difference is that the Android build does not include the serial diagnostic page.

## 9. Troubleshooting

If the Qt app opens but cannot connect to MQTT:

- verify the board can reach the broker
- verify `TLS` is enabled in the Qt settings page
- verify device ID is `relay_h743_001`
- check `运行日志` for broker errors

If commands publish but no ACK arrives:

- open `串口诊断`
- run `STATUS`
- run `MQTTINIT` again after `NETINIT`
- confirm the board is subscribed to `device/relay_h743_001/down/cmd`

If the backend login fails:

- start the backend first
- confirm `http://127.0.0.1:8000/api/v1/devices` is reachable
- confirm `backend\.env` still uses `QXZ_ADMIN_USERNAME=admin`

If the desktop app cannot start:

- run `powershell -ExecutionPolicy Bypass -File "D:\Competition\code_main\qixiangzhan\scripts\check_qt_env.ps1"`
- run `powershell -ExecutionPolicy Bypass -File "D:\Competition\code_main\qixiangzhan\scripts\deploy_desktop_qt.ps1"`
- confirm `qt-client\build\desktop-verify\relay_qt_client.exe` exists
- rebuild with `cmake --preset desktop-mingw`

## 10. Recommended first full end-to-end test

1. Power the H743 + L610 hardware.
2. Use `串口诊断` and run `SELFTEST`, `NETINIT`, `MQTTINIT`, `STATUS`.
3. Start the backend and desktop client with `scripts\start_desktop_stack.cmd`.
4. Log in with `admin / admin123`.
5. In `设置`, connect MQTT using the defaults above.
6. In `设备总览`, select `relay_h743_001`.
7. Click `查询当前设备`.
8. Click `R1 ON` and verify relay action, ACK, and history entry.
9. Click `R1 OFF`, then repeat with relay 2.
10. Open `告警` and `历史` to verify backend and local cache are both updating.
