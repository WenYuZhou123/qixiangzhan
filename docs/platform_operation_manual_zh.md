# 气象站平台完整操作手册

本文档适用于当前仓库中的整套平台：

- 硬件：`STM32H743 + L610`
- Broker：`EMQX`
- 桌面端：`Qt Desktop`
- 安卓端：`Qt Android`
- 后端：`FastAPI + MySQL`
- 本地缓存：`SQLite`
- 数据库管理工具：`Navicat`

仓库根目录：

```text
D:\Competition\code_main\qixiangzhan
```

## 1. 当前默认参数

当前工程默认参数如下：

- 设备 ID：`relay_h743_001`
- MQTT Host：`rc11adc1.ala.cn-hangzhou.emqxsl.cn`
- MQTT Port：`8883`
- MQTT 用户名：`h743`
- MQTT 密码：`123456`
- MQTT TLS：`开启`
- 后端 API（本地联调）：`http://127.0.0.1:8000/api/v1`
- 后端 API（公网正式环境）：`https://qixiangzhan.online/api/v1`
- 后端登录账号：`admin`
- 后端登录密码：`admin123`
- 串口波特率：`115200`
- MySQL 库名：`qixiangzhan`
- MySQL 用户：`qixiang_app`

支持的命令：

- `set_r1`
- `set_r2`
- `set_all`
- `query_status`

旧固件兼容：

- 旧状态 topic：`device/relay/status`
- 旧命令 topic：`device/relay/cmd`
- 转义 JSON 状态报文已兼容

## 2. 目录说明

你最常用的目录和文件：

- `scripts\check_qt_env.ps1`：检查 Qt/Android 工具链
- `scripts\start_desktop_stack.cmd`：一键启动桌面联调栈
- `scripts\start_desktop_stack.ps1`：同上，PowerShell 版本
- `scripts\build_android_apk.cmd`：生成安卓 APK
- `scripts\generate_android_keystore.cmd`：生成安卓 release 签名 keystore
- `scripts\deploy_desktop_qt.ps1`：补齐桌面端 Qt 运行库
- `backend\`：后端服务
- `qt-client\`：Qt 客户端源码
- `docs\navicat_mysql_setup.md`：Navicat + MySQL 说明

## 3. 首次使用前准备

### 3.1 检查 Qt 环境

在 PowerShell 中执行：

```powershell
powershell -ExecutionPolicy Bypass -File "D:\Competition\code_main\qixiangzhan\scripts\check_qt_env.ps1"
```

你应该重点确认以下项目存在：

- `D:\Qt\6.7.3\mingw_64`
- `D:\Qt\6.7.3\android_arm64_v8a`
- `Qt6Mqtt`
- `Qt6SerialPort`
- Android SDK / NDK

### 3.2 初始化后端 Python 环境

进入后端目录：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\backend
```

如果 `.venv` 还没有，先创建：

```powershell
py -3.14 -m venv .venv
```

安装依赖：

```powershell
.venv\Scripts\pip.exe install -r requirements.txt
```

### 3.3 配置 MySQL 与 Navicat

当前后端默认使用 MySQL，而不是 PostgreSQL。

请先在 Navicat 或 MySQL 中创建数据库：

```sql
CREATE DATABASE qixiangzhan
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;
```

创建用户：

```sql
CREATE USER 'qixiang_app'@'%' IDENTIFIED BY 'change-me';
GRANT ALL PRIVILEGES ON qixiangzhan.* TO 'qixiang_app'@'%';
FLUSH PRIVILEGES;
```

如果后端只在本机使用，可以把 `'%'` 改成 `'localhost'`。

### 3.4 配置后端 `.env`

复制环境文件：

```powershell
Copy-Item .env.example .env
```

把 `backend\.env` 中数据库连接改成你的实际密码，例如：

```text
QXZ_DATABASE_URL=mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/qixiangzhan?charset=utf8mb4
```

如果 MQTT、管理员账号、超时时间需要调整，也在这里改。

正式上云时，建议至少改成下面这一组：

```text
QXZ_API_HOST=0.0.0.0
QXZ_API_PORT=8000
QXZ_PUBLIC_API_BASE=https://qixiangzhan.online/api/v1
QXZ_DATABASE_URL=mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/qixiangzhan?charset=utf8mb4
QXZ_MQTT_HOST=rc11adc1.ala.cn-hangzhou.emqxsl.cn
QXZ_MQTT_PORT=8883
QXZ_MQTT_USE_TLS=true
QXZ_TOKEN_SECRET=replace-with-a-long-random-secret
QXZ_JSON_LOGS=true
```

### 3.5 QtKeychain 与安卓签名准备

当前客户端已经把敏感登录凭据切到系统安全存储：

- `access_token`
- `refresh_token`
- `access_expires_at`
- `refresh_expires_at`

这些字段不再长期保存在 SQLite 明文里，而是优先写入系统 keychain。

安卓 release 签名用的 keystore 默认放在仓库外：

```text
C:\Users\Lenovo\.qixiangzhan\android\release\qixiangzhan-release.jks
```

首次生成 keystore：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\generate_android_keystore.cmd
```

脚本还会在同目录生成仅供本机使用的环境文件：

- `release-signing.env.ps1`
- `release-signing.env.cmd`

不要把 keystore 或这些环境文件提交到 Git。

## 4. 推荐启动顺序

推荐你每次都按这个顺序操作：

1. 开发板上电
2. 串口检查模块联网与 MQTT
3. 启动后端
4. 启动 Qt 桌面端
5. 登录
6. 连接 MQTT
7. 查看设备状态
8. 测试继电器控制
9. 打开 Navicat 验证是否入库

### 4.1 气象传感器实物接线注意

当前工程里空气传感器和风速模块的实物联调建议如下：

- `CJ702-U` 空气传感器：
  - `VDD` -> 传感器供电正
  - `GND` -> 传感器供电地，并与 STM32 共地
  - `TX` -> `USART3_RX (PB11)`
  - `NC` 悬空
- 如果 `CJ702-U` 的 `TX` 是 5V TTL，请先做电平保护，再接 STM32。
- `ZTS-3000-FSJT-V02` 风速模块：
  - 棕 -> `10~30V` 电源正
  - 黑 -> 电源负
  - 蓝 -> 输出正
  - 黄/绿 -> 输出负
- 当前风速代码只适用于“已经做过差分转单端”的模拟前端场景。
  在没有确认前端电路和参考地关系之前，不要把原始输出线直接接到 STM32 ADC。

## 5. 一键启动桌面联调栈

最推荐的启动方式：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\start_desktop_stack.cmd
```

或者：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
powershell -ExecutionPolicy Bypass -File .\scripts\start_desktop_stack.ps1
```

这个脚本会做三件事：

- 启动 FastAPI 后端
- 检查并补齐 Qt 桌面运行库
- 启动桌面端 `relay_qt_client.exe`

如果你在别的目录执行，请使用绝对路径：

```powershell
powershell -ExecutionPolicy Bypass -File "D:\Competition\code_main\qixiangzhan\scripts\start_desktop_stack.ps1"
```

## 6. 手动启动方式

如果你不想用一键脚本，可以手动分别启动。

### 6.1 启动后端

```powershell
Set-Location D:\Competition\code_main\qixiangzhan\backend
.venv\Scripts\python.exe -m uvicorn app.main:app --host 127.0.0.1 --port 8000 --reload
```

### 6.2 启动桌面客户端

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\qt-client\build\desktop-verify\relay_qt_client.exe
```

如果出现 `Qt6Core.dll` 缺失，执行：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
powershell -ExecutionPolicy Bypass -File .\scripts\deploy_desktop_qt.ps1
```

## 7. 桌面端登录与页面使用

### 7.1 登录

启动软件后：

1. 打开“登录”页
2. 确认 `API Base` 为 `http://127.0.0.1:8000/api/v1`
3. 输入账号密码：`admin / admin123`
4. 点击登录

如果后端没启动，某些旧版本会回退到本地管理员模式；当前建议始终启动后端后再登录。

### 7.2 设置页

在“设置”页中：

- 可以修改 `API Base`
- 可以看到本地 SQLite 路径
- 可以看到云端同步状态

桌面端在设置页中还会显示 MQTT 连接配置区。

### 7.3 MQTT 连接

在桌面端设置页填写：

- Host：`rc11adc1.ala.cn-hangzhou.emqxsl.cn`
- Port：`8883`
- Username：`h743`
- Password：`123456`
- Current Device：`relay_h743_001`
- TLS：开启

然后点击“连接”。

连接成功后，顶部状态通常会显示 `Connected`。

### 7.4 设备总览

打开“设备总览”页后可以：

- 查看设备列表
- 选择设备
- 查看在线状态
- 查看 Relay1 / Relay2 状态
- 查看 RSSI / 运营商 / IP / Tick / Timestamp
- 查看最近 ACK 和错误

常用按钮：

- `查询当前设备`
- `R1 ON`
- `R1 OFF`
- `R2 ON`
- `R2 OFF`
- `全部打开`
- `全部关闭`
- `重试上次命令`

### 7.5 历史页

“命令与消息历史”页会显示：

- 串口消息
- MQTT 收发
- 后端同步回来的历史消息
- 命令执行结果

桌面端优先保留本地缓存，云端历史会覆盖到本地远程缓存中。

### 7.6 告警页

“告警中心”会显示：

- 设备离线
- ACK 超时
- 命令错误
- MQTT 断连
- 低 RSSI

### 7.7 运行日志

“运行日志”主要用于联调观察：

- MQTT 连接过程
- 订阅情况
- 收到的 topic
- 下发的命令
- API 请求日志

如果后面联调有问题，优先把这个页面截图给我。

## 8. 串口联调操作

桌面端专属页面：“串口诊断”。

操作步骤：

1. 点击“刷新串口”
2. 选择开发板对应的 `COM` 口
3. 保持波特率 `115200`
4. 点击“连接”

推荐第一次上电后按这个顺序执行：

1. `SELFTEST`
2. `NETINIT`
3. `MQTTINIT`
4. `STATUS`

预设指令说明：

- `ATRAW`：原始 AT 调试
- `SELFTEST`：自检
- `NETINIT`：蜂窝网络初始化
- `MQTTINIT`：MQTT 建链
- `MQTTCLOSE`：关闭 MQTT
- `STATUS`：查看当前状态

如果你要直接验证继电器，也可发送：

- `R1_ON`
- `R1_OFF`
- `R2_ON`
- `R2_OFF`
- `ALL_ON`
- `ALL_OFF`
- `STATUS`

## 9. EMQX 主题说明

### 9.1 标准协议

- `device/<device_id>/down/cmd`
- `device/<device_id>/up/status`
- `device/<device_id>/up/ack`
- `device/<device_id>/up/event`
- `device/<device_id>/up/online`

以当前默认设备为例：

- `device/relay_h743_001/down/cmd`
- `device/relay_h743_001/up/status`
- `device/relay_h743_001/up/ack`
- `device/relay_h743_001/up/event`
- `device/relay_h743_001/up/online`

### 9.2 旧固件协议

当前平台也兼容旧固件：

- 状态：`device/relay/status`
- 命令：`device/relay/cmd`

如果板子仍然使用旧 topic，桌面端和后端会自动兼容。

## 10. 数据入库与 Navicat 验证

后端启动后会自动建表。

### 10.1 应该看到的表

在 Navicat 中确认这几个表存在：

- `users`
- `devices`
- `device_last_state`
- `telemetry_messages`
- `command_messages`
- `alarms`

### 10.2 正常运行时各表变化

当开发板在线并持续上报时：

- `devices`：设备最新聚合状态会更新
- `device_last_state`：保存最新规范化状态
- `telemetry_messages`：持续新增原始 MQTT 消息
- `command_messages`：桌面端或安卓端发命令时增长
- `alarms`：离线、低 RSSI、ACK 超时时变化

### 10.3 常用 SQL

查看设备最新状态：

```sql
SELECT device_id, display_name, online, relay1, relay2, rssi, operator_name, ip, updated_at
FROM devices
ORDER BY updated_at DESC;
```

查看最近命令：

```sql
SELECT msg_id, device_id, command, value, status, detail, created_at, acked_at
FROM command_messages
ORDER BY created_at DESC
LIMIT 50;
```

查看活跃告警：

```sql
SELECT device_id, code, severity, message, source, created_at
FROM alarms
WHERE active = 1
ORDER BY created_at DESC;
```

查看原始遥测：

```sql
SELECT device_id, topic, direction, event_type, payload, created_at
FROM telemetry_messages
ORDER BY created_at DESC
LIMIT 100;
```

## 11. 安卓 App 操作

安卓端使用同一套 Qt 工程构建，但行为与桌面端不同：

- 安卓端不接串口
- 安卓端不直接连 MQTT
- 安卓端通过后端 API 轮询设备状态与历史
- 安卓端命令通过后端转发到 MQTT

### 11.1 生成 APK

Debug 包：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\build_android_apk.cmd
```

生成后的 APK 路径：

```text
D:\Competition\code_main\qixiangzhan\qt-client\build\android-arm64\android-build\build\outputs\apk\debug\android-build-debug.apk
```

### 11.2 生成已签名 Release APK

正式安卓包固定参数：

- 包名：`com.qixiangzhan.platform`
- 应用名：`气象站控制平台`
- `versionCode`：`1`
- `versionName`：`1.0.0`

首次签名建议先执行：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\generate_android_keystore.cmd
```

然后生成 release 包：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\build_android_apk.cmd -Configuration Release
```

如果要把 release 默认后端改成公网域名，先设置环境变量：

```powershell
$env:QXZ_CLIENT_DEFAULT_API_BASE = "https://qixiangzhan.online/api/v1"
.\scripts\build_android_apk.cmd -Configuration Release
```

生成后的已签名 release APK 路径：

```text
D:\Competition\code_main\qixiangzhan\qt-client\build\android-arm64-release\android-build\build\outputs\apk\release\android-build-release-signed.apk
```

脚本会自动执行 `apksigner verify --verbose` 验证签名。

### 11.3 安装前注意事项

安卓手机必须能访问后端 API。

如果只是本地联调，且后端还在你电脑本机 `127.0.0.1:8000`：

- 手机不能用 `127.0.0.1`
- 必须把 `API Base` 改成你电脑在局域网里的 IP，例如：
  - `http://192.168.1.20:8000/api/v1`

同时要确认：

- 电脑与手机在同一局域网
- Windows 防火墙允许 `8000` 端口
- 启动后端时不要只绑定到不可访问地址

如果要跨不同局域网，正式方案是：

- 把后端部署到云服务器
- 域名解析 `qixiangzhan.online -> 云服务器公网 IP`
- 用 Caddy 提供 `HTTPS`
- 桌面端和安卓端都把 `API Base` 设为 `https://qixiangzhan.online/api/v1`

### 11.4 安卓端使用流程

1. 安装 APK
2. 打开 App
3. 在登录页配置 `API Base`
4. 使用 `admin / admin123` 登录
5. 进入设备总览
6. 等待设备轮询刷新
7. 点击命令按钮测试控制
8. 查看历史和告警是否同步

安卓端默认轮询策略：

- 设备页：约每 5 秒
- 历史 / 告警 / 日志 / 设置：约每 15 秒
- 命令状态轮询：约每 3 秒，最长 20 秒

### 11.5 安卓登录状态与安全存储

安卓端会把登录凭据优先存入系统安全存储，而不是 SQLite 明文。

你需要知道的行为：

- 第一次登录成功后，重开 App 通常可以恢复会话
- 点击“退出登录”后，会删除本地保存的 token
- 如果安全存储不可用，本次登录只保存在内存里，关闭 App 后需要重新登录
- Android release 默认应该连接公网 API，不建议继续把 `127.0.0.1` 当作正式环境地址
- 推荐正式环境域名：`https://qixiangzhan.online/api/v1`

## 12. 推荐完整联调流程

建议你第一次按下面顺序做整套验证：

### 阶段 A：硬件与 MQTT

1. 开发板上电
2. 打开“串口诊断”
3. 执行 `SELFTEST`
4. 执行 `NETINIT`
5. 执行 `MQTTINIT`
6. 执行 `STATUS`
7. 确认串口显示网络和 MQTT 正常

### 阶段 B：桌面端控制

1. 启动后端和桌面端
2. 登录 `admin / admin123`
3. 在“设置”页连接 MQTT
4. 打开“设备总览”
5. 点击“查询当前设备”
6. 确认设备在线、RSSI/IP 正常显示
7. 点击 `R1 ON`
8. 点击 `R1 OFF`
9. 点击 `R2 ON`
10. 点击 `R2 OFF`

### 阶段 C：数据库入库

1. 打开 Navicat
2. 查看 `devices`
3. 查看 `telemetry_messages`
4. 查看 `command_messages`
5. 查看 `alarms`
6. 确认数据持续增长

### 阶段 D：安卓端

1. 构建 APK
2. 安装到手机
3. 把 `API Base` 改成电脑局域网 IP
4. 登录
5. 查看设备状态
6. 下发命令
7. 检查 Navicat 中是否写入对应记录

## 13. 常见问题排查

### 13.1 PowerShell 找不到脚本

你必须先进入项目目录，或者使用绝对路径。

正确示例：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\start_desktop_stack.cmd
```

或者：

```powershell
powershell -ExecutionPolicy Bypass -File "D:\Competition\code_main\qixiangzhan\scripts\start_desktop_stack.ps1"
```

### 13.2 Qt 软件提示缺少 `Qt6Core.dll`

执行：

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
powershell -ExecutionPolicy Bypass -File .\scripts\deploy_desktop_qt.ps1
```

### 13.3 顶部显示 `Connected`，但设备还是 `OFFLINE`

优先检查：

- 板子是否真正发出状态消息
- 运行日志里是否有 `RX device/.../status`
- 当前设备 ID 是否选对
- 板子是否仍在用旧 topic

### 13.4 命令发送后一直 ACK 超时

优先检查：

- 串口里是否执行了 `MQTTINIT`
- 板子是否订阅了命令 topic
- EMQX 中设备是否在线
- 运行日志里是否出现：
  - `TX ...`
  - `RX ...ack`
  - 或 `Detected legacy topic ...`

### 13.5 登录失败或接口 401

检查：

- 后端是否已启动
- `API Base` 是否正确
- 是否真的使用了 `admin / admin123`
- token 是否过期或为空

必要时重新登录。

### 13.6 后端启动时报 MySQL 错误

检查：

- MySQL 服务是否启动
- `qixiangzhan` 数据库是否存在
- `qixiang_app` 用户是否存在
- `.env` 中用户名密码是否正确
- Navicat 是否能连通

### 13.7 安卓能打开但看不到数据

最常见原因：

- 安卓 `API Base` 还写着 `127.0.0.1`
- 手机访问不到电脑的 `8000` 端口
- 电脑防火墙拦截

解决方法：

1. 把 API 地址改为电脑局域网 IP
2. 确认电脑和手机在同一网络
3. 确认后端正在运行

## 14. 建议你平时怎么用

日常最省事的方式：

1. 开发板上电
2. 运行 `scripts\start_desktop_stack.cmd`
3. 登录
4. 连接 MQTT
5. 在设备页控制
6. 在 Navicat 看入库

只有在以下情况再单独打开串口页：

- 首次上电
- 网络没起来
- MQTT 没连上
- ACK 超时
- 怀疑板子命令 topic 有问题

如果是正式异地使用：

1. 先把后端部署到云服务器
2. 配置 `qixiangzhan.online`
3. 让桌面端和安卓端统一连接 `https://qixiangzhan.online/api/v1`
4. Windows 工程模式只保留给现场调试

## 15. 相关文档

- `docs\navicat_mysql_setup.md`
- `docs\h743_l610_qt_quick_start.md`
- `docs\public_remote_deployment_guide.md`
- `backend\README.md`
- `qt-client\README.md`

如果你愿意，我下一步可以继续给你补一份“现场联调检查清单”，做成勾选式版本，你拿着它就能一项一项排查。
