# Qt Desktop + Android Client

Qt 6 control platform for the `STM32H743 + L610 + EMQX` hardware project.

Remote/public deployment guide: `..\docs\public_remote_deployment_guide.md`

Full manual: `..\docs\platform_operation_manual_zh.md`

## Current scope

- Qt/QML desktop shell with:
  - 登录页
  - 设备总览 + 设备详情
  - 命令/消息历史
  - 告警中心
  - 设置页
  - 串口诊断
  - 运行日志
- 共享业务层:
  - `AuthSession`
  - `DeviceRepository`
  - `RealtimeGateway`
  - `HistoryRepository`
  - `AlarmRepository`
  - `SerialConsoleService`
- 本地 SQLite:
  - `devices_cache`
  - `last_state_cache`
  - `message_cache`
  - `command_outbox`
  - `alarm_cache`
  - `app_settings`

## Build

Desktop:

```powershell
D:\Qt\6.7.3\mingw_64\bin\qt-cmake.bat -S . -B build\desktop -G "MinGW Makefiles" `
  -DCMAKE_C_COMPILER=D:/Qt/Tools/mingw1120_64/bin/gcc.exe `
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1120_64/bin/g++.exe
cmake --build build\desktop
```

Or use presets:

```powershell
cmake --preset desktop-mingw
cmake --build --preset build-desktop-mingw
```

Android arm64:

```powershell
cmake --preset android-arm64
cmake --build --preset build-android-arm64
..\scripts\build_android_apk.cmd
```

Android arm64 release (signed):

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\generate_android_keystore.cmd
.\scripts\build_android_apk.cmd -Configuration Release
```

Override the default API base for release:

```powershell
$env:QXZ_CLIENT_DEFAULT_API_BASE = "https://api.qixiangzhan.online/api/v1"
.\scripts\build_android_apk.cmd -Configuration Release
```

## Environment helpers

- `..\scripts\check_qt_env.ps1`
- `..\scripts\repair_qt_modules.ps1`
- `..\scripts\deploy_desktop_qt.ps1`
- `..\scripts\start_desktop_stack.ps1`

## Quick start

- Hardware-specific guide: `..\docs\h743_l610_qt_quick_start.md`
- One-click launch:

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\start_desktop_stack.cmd
```

## Notes

- Desktop MQTT needs Qt MQTT.
- Desktop serial diagnostics needs Qt SerialPort.
- Android release should use the public API/WebSocket path instead of direct MQTT.
- If Android Qt MQTT is unavailable, keep the shared UI/data layer and swap the realtime path to FastAPI/WebSocket later.

## Secure session storage

- `QtKeychain` is vendored under `third_party/qtkeychain` and built as part of the client.
- `access_token`, `refresh_token`, `access_expires_at`, and `refresh_expires_at` are stored in the OS keychain instead of SQLite.
- Existing plaintext token fields in SQLite are migrated once on startup and then removed.
- If the keychain backend is unavailable, the current login stays in memory only and the UI reports that the session will not be remembered.

Non-sensitive configuration still stays in SQLite:

- `api.baseUrl`
- `auth.username`
- `auth.displayName`
- `auth.role`
- `app.engineeringMode`

## Android release signing

The formal Android package is now:

- `applicationId`: `com.qixiangzhan.platform`
- `app label`: `气象站控制平台`
- `versionCode`: `1`
- `versionName`: `1.0.0`

Default keystore location:

```text
C:\Users\Lenovo\.qixiangzhan\android\release\qixiangzhan-release.jks
```

Default alias:

```text
qixiangzhan-release
```

Signing inputs are read from environment variables:

- `QXZ_ANDROID_KEYSTORE_PATH`
- `QXZ_ANDROID_KEYSTORE_ALIAS`
- `QXZ_ANDROID_KEYSTORE_STORE_PASS`
- `QXZ_ANDROID_KEYSTORE_KEY_PASS`
- `QXZ_CLIENT_DEFAULT_API_BASE`

Local helper profiles are written outside the repo:

- `C:\Users\Lenovo\.qixiangzhan\android\release\release-signing.env.ps1`
- `C:\Users\Lenovo\.qixiangzhan\android\release\release-signing.env.cmd`

Signed release APK output:

```text
D:\Competition\code_main\qixiangzhan\qt-client\build\android-arm64-release\android-build\build\outputs\apk\release\android-build-release-signed.apk
```

The release script automatically runs `apksigner verify --verbose` after packaging.

## Remote-ready behavior

- Desktop defaults to API-first remote mode after backend login.
- Windows engineering mode keeps serial diagnostics and direct MQTT for lab use.
- Android uses HTTPS API and falls back to REST polling if WebSocket is unavailable.
- Recommended production API base: `https://api.qixiangzhan.online/api/v1`
