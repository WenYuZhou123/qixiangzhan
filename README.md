# qixiangzhan

基于 `STM32H743 + L610 + EMQX + FastAPI + Qt` 的气象站与远程控制平台工程。

当前项目已经形成一套完整的“三端 + 后端 + 云链路”架构：

- 本地设备端：`STM32H743 + L610 + LCD`
- 平台后端：`FastAPI + MySQL`
- 客户端：`Qt Desktop + Qt Android`

项目目标不是单一的传感器采集，而是把设备状态、气象数据、远程控制、历史追溯和现场调试整合成一套可演示、可联调、可部署的平台。

## 当前状态

当前板卡已经接入并上报的真实测量项：

- 风速
- 风向
- 温度
- 湿度
- 雨滴检测 / 雨量湿润度
- PM2.5
- PM10
- CO2
- TVOC
- CH2O

当前未接入的测量项：

- 气压
- 能见度

项目已经在固件 MQTT 状态报文、后端聚合层、Qt 缓存层和三端 UI 中统一接入 `weather.capabilities` 能力标记。
未接入项不会再被当作 `0` 值实测数据显示。

## 系统架构

```text
STM32H743 + Sensor + LCD
        │
        │ MQTT / 4G
        ▼
      EMQX
        │
        ├──────────────► Qt Desktop
        │                 - 远程模式：API + Realtime
        │                 - 工程模式：串口 + MQTT + 本地联调
        │
        └────► FastAPI + MySQL + WebSocket
                      │
                      └──────────────► Qt Android
                                        - API + WebSocket / Polling
```

## 三端说明

### 1. 本地 LCD

- 用于现场查看设备状态、气象摘要、继电器控制和底层调试
- 维持 `OVERVIEW / CONTROL / WEATHER / DEBUG` 四页结构
- 与桌面端、安卓端保持一致的状态语义

### 2. Qt 桌面端

- 主界面包含概览、控制、气象、历史、告警、设置
- 工程模式下保留串口调试、本地 MQTT 和日志能力
- 用于演示、值守和现场联调

### 3. Qt 安卓端

- 复用桌面端核心 QML 页面
- 通过后端 API / WebSocket 获取状态和下发命令
- 不直接连接串口，不承担低层调试职责

## 项目结构

```text
qixiangzhan/
├─ Core/              STM32Cube 生成的核心代码
├─ HARDWARE/          L610、传感器、LCD、业务逻辑
├─ Drivers/           STM32 HAL / CMSIS
├─ MDK-ARM/           Keil 工程
├─ backend/           FastAPI 后端
├─ qt-client/         Qt 桌面端与安卓端
├─ deploy/linux/      Linux 云端部署模板
├─ docs/              操作手册、部署文档、联调说明
└─ scripts/           启动、构建、检查脚本
```

## 功能概览

- 设备通过 MQTT 上报：
  - 在线状态
  - 遥测数据
  - ACK
  - 事件
- 后端负责：
  - 用户鉴权
  - 设备状态聚合
  - 命令转发
  - 历史入库
  - WebSocket 实时推送
- Qt 客户端支持：
  - 登录鉴权
  - 设备概览与详情
  - 命令下发
  - 历史记录
  - 告警中心
  - 设置与缓存
- 桌面工程模式额外支持：
  - 串口诊断
  - 现场 MQTT 调试
  - 运行日志查看

## 默认参数

- 设备 ID：`relay_h743_001`
- MQTT Host：`rc11adc1.ala.cn-hangzhou.emqxsl.cn`
- MQTT Port：`8883`
- MQTT Username：`h743`
- MQTT Password：`123456`
- 本地联调 API：`http://127.0.0.1:8000/api/v1`
- 公网正式 API：`https://api.qixiangzhan.online/api/v1`
- 公网实时地址：`wss://api.qixiangzhan.online/api/v1/ws/realtime`
- 后端管理员账号：`admin / admin123`

## 快速开始

### 1. 启动后端

```powershell
Set-Location .\backend
py -3.14 -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
Copy-Item .env.example .env
py -3.14 -m uvicorn app.main:app --reload
```

### 2. 启动桌面联调栈

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\start_desktop_stack.cmd
```

该脚本会完成：

- 启动 FastAPI
- 检查并补齐桌面 Qt 运行库
- 启动桌面客户端

### 3. 构建安卓 APK

```powershell
Set-Location D:\Competition\code_main\qixiangzhan
.\scripts\build_android_apk.cmd
```

如需默认接入公网 API：

```powershell
$env:QXZ_CLIENT_DEFAULT_API_BASE = "https://api.qixiangzhan.online/api/v1"
.\scripts\build_android_apk.cmd -Configuration Release
```

## 推荐联调顺序

1. 开发板上电
2. 确认 L610 联网和 MQTT 正常
3. 启动后端
4. 启动桌面端
5. 登录并查看设备状态
6. 执行继电器或停机坪控制
7. 验证历史、告警和数据库入库
8. 最后再验证安卓端访问

## 关键设计说明

### 气象能力标记

设备状态报文中的 `weather.capabilities` 当前固定包含：

- `wind`
- `air`
- `rain`
- `pressure`
- `visibility`

当前 H743/L610 板卡默认值：

```json
{
  "wind": true,
  "air": true,
  "rain": true,
  "pressure": false,
  "visibility": false
}
```

用途：

- 固件明确告诉上层哪些字段是真实接入的
- 后端聚合和数据库不会再把“默认 0”误解为“真实测量”
- Qt 桌面 / 安卓 / LCD 只展示真实接入项

### UI 收尾原则

- 三端统一状态语义
- 不展示未接入测量项
- 主页面强调状态和控制，不堆调试信息
- 调试信息留在工程页、日志页和 LCD Debug 页

## 文档入口

- [docs/platform_operation_manual_zh.md](docs/platform_operation_manual_zh.md)
- [docs/h743_l610_qt_quick_start.md](docs/h743_l610_qt_quick_start.md)
- [docs/public_remote_deployment_guide.md](docs/public_remote_deployment_guide.md)
- [docs/jetson_edge_master_guide_zh.md](docs/jetson_edge_master_guide_zh.md)
- [backend/README.md](backend/README.md)
- [qt-client/README.md](qt-client/README.md)

## 仓库协作约定

建议提交到 Git 的内容：

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
