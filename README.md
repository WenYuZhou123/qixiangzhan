# qixiangzhan

基于 `STM32H743 + L610 + EMQX + FastAPI + Qt` 的气象站/远程控制平台工程。

这个仓库包含三部分能力：

- 设备端固件：`STM32H743 + L610`，通过公网 `EMQX` 上报状态、接收命令
- 平台后端：`FastAPI + MySQL`，负责鉴权、设备数据汇总、命令下发、WebSocket 实时推送
- 客户端：`Qt Desktop + Qt Android`，支持设备查看、历史记录、告警、远程控制；桌面端保留工程模式下的串口与 MQTT 调试

## 项目结构

```text
qixiangzhan/
├─ Core/              STM32Cube 生成的核心代码
├─ HARDWARE/          L610、协议、传感器、业务模块
├─ Drivers/           STM32 HAL / CMSIS
├─ MDK-ARM/           Keil 工程
├─ backend/           FastAPI 后端
├─ qt-client/         Qt 桌面端与安卓端
├─ deploy/linux/      Linux 云服务器部署模板
├─ docs/              中文操作手册、部署文档、联调说明
└─ scripts/           本地启动、打包、环境检查脚本
```

## 功能概览

- 设备通过 MQTT 上报在线状态、遥测数据、ACK、事件
- 后端写入 MySQL，并通过 `WS /api/v1/ws/realtime` 推送实时消息
- Qt 客户端支持：
  - 登录鉴权
  - 设备总览与详情
  - 命令/消息历史
  - 告警中心
  - 安卓远程访问
  - Windows 工程模式串口诊断
- 支持跨局域网访问：
  - 正式环境 API：`https://api.qixiangzhan.online/api/v1`
  - 正式环境实时地址：`wss://api.qixiangzhan.online/api/v1/ws/realtime`

## 当前默认参数

- 设备 ID：`relay_h743_001`
- MQTT Host：`rc11adc1.ala.cn-hangzhou.emqxsl.cn`
- MQTT Port：`8883`
- MQTT Username：`h743`
- MQTT Password：`123456`
- 本地联调 API：`http://127.0.0.1:8000/api/v1`
- 公网正式 API：`https://api.qixiangzhan.online/api/v1`
- 后端管理员账号：`admin / admin123`

## 快速开始

### 1. 后端

```powershell
Set-Location .\backend
py -3.14 -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
Copy-Item .env.example .env
py -3.14 -m uvicorn app.main:app --reload
```

### 2. 桌面端一键联调

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\start_desktop_stack.cmd
```

### 3. 安卓 APK 构建

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\build_android_apk.cmd
```

安卓 release 构建前，如需默认接入公网地址：

```powershell
$env:QXZ_CLIENT_DEFAULT_API_BASE = "https://api.qixiangzhan.online/api/v1"
.\scripts\build_android_apk.cmd -Configuration Release
```

## 公网部署

推荐部署模型：

- 云服务器运行 `FastAPI + MySQL + Caddy`
- 域名 `api.qixiangzhan.online` 指向云服务器公网 IP
- Caddy 提供 HTTPS 反向代理
- 设备继续连公网 EMQX
- Android 与 Windows 普通用户统一走 `HTTPS API + WebSocket`

部署参考：

- [docs/public_remote_deployment_guide.md](docs/public_remote_deployment_guide.md)
- [deploy/linux/README.md](deploy/linux/README.md)

## 主要文档

- [docs/platform_operation_manual_zh.md](docs/platform_operation_manual_zh.md)
- [docs/h743_l610_qt_quick_start.md](docs/h743_l610_qt_quick_start.md)
- [backend/README.md](backend/README.md)
- [qt-client/README.md](qt-client/README.md)

## GitHub 仓库建议

建议把以下内容提交到 GitHub：

- 固件源码
- `backend/` 后端源码
- `qt-client/` 客户端源码
- `docs/` 文档
- `deploy/linux/` 部署模板
- `scripts/` 辅助脚本

不建议提交：

- `backend/.venv/`
- `qt-client/build/`
- keystore、签名文件、`.env`
- 临时日志和本地缓存
