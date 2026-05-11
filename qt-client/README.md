# Qt Desktop + Android Client

Qt 6 客户端同时服务 Windows 桌面端和 Android App。桌面端偏现场值守和工程调试，Android 端偏移动查看、告警确认和基础控制。

默认生产 API：

```text
https://qixiangzhan.online/api/v1
```

默认实时地址由客户端自动从 API 地址推导：

```text
wss://qixiangzhan.online/api/v1/ws/realtime
```

## 构建桌面端

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\qt-client
cmake --preset desktop-mingw
cmake --build --preset build-desktop-mingw
```

一键启动桌面联调栈：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\start_desktop_stack.cmd
```

## 构建 Android

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\qt-client
cmake --preset android-arm64
cmake --build --preset build-android-arm64
```

打包 APK：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\build_android_apk.cmd
```

Release 包可显式指定 API：

```powershell
$env:QXZ_CLIENT_DEFAULT_API_BASE = "https://qixiangzhan.online/api/v1"
.\scripts\build_android_apk.cmd -Configuration Release
```

## 运行模式

- 桌面端默认 API-first，可通过 HTTPS/WebSocket 连接 Jetson 后端。
- 桌面端工程模式保留串口诊断、本地日志和现场调试入口。
- Android 端只走 HTTPS API 和 WebSocket/REST 轮询，不直接连接 MQTT、串口或 MySQL。
- 登录页和设置页可以覆盖 `API Base`，覆盖值会保存到本地 SQLite 配置。

## 关键模块

- `AuthSession`：登录、Token 刷新、安全存储。
- `ApiClient`：统一 REST 请求、Bearer Token 注入。
- `RemoteSyncService`：设备、气象、告警、命令、系统健康和 WebSocket 实时数据。
- `DeviceStateStore`：设备状态模型和 QML 展示数据。
- `SerialConsoleService`：桌面端工程串口诊断。
- `third_party/qtkeychain`：桌面端和 Android 安全凭据存储。

## Android 注意事项

- `android/AndroidManifest.xml` 已包含 `INTERNET` 权限。
- Android 正式包使用公网 HTTPS API，不需要局域网 IP。
- 如果 WebSocket 不可用，客户端会保留 REST 轮询能力。
- Release 签名配置存放在本机用户目录，不提交到仓库。

## 本地缓存

非敏感配置保存在 SQLite：

- `api.baseUrl`
- `auth.username`
- `auth.displayName`
- `auth.role`
- `app.engineeringMode`

敏感 Token 优先保存到系统 keychain；如果 keychain 不可用，只保留当前内存会话并在 UI 中提示。

## 辅助脚本

- `../scripts/check_qt_env.ps1`
- `../scripts/repair_qt_modules.ps1`
- `../scripts/deploy_desktop_qt.ps1`
- `../scripts/start_desktop_stack.ps1`
- `../scripts/build_android_apk.ps1`
- `../scripts/generate_android_keystore.ps1`
