# 仓库代码地图

这个仓库是一个混合工程：既包含 STM32 固件，也包含 Jetson 后端、Qt 桌面端、Android App、部署脚本和文档。为了避免破坏工具链路径，本仓库采用保守目录布局。

## 工具链敏感目录

这些目录和文件与 STM32CubeMX、Keil、HAL/CMSIS 路径强相关，不要随意移动或改名：

- `qixiangzhan.ioc`：STM32CubeMX 工程入口。
- `Core/`：CubeMX 生成的 MCU 初始化和中断代码。
- `Drivers/`：STM32 HAL、CMSIS 和设备支持包。
- `MDK-ARM/`：Keil 工程、启动文件、链接脚本和调试配置。
- `.mxproject`：CubeMX 关联元数据。

如果必须移动这些目录，需要同步更新 CubeMX、Keil 工程和包含路径，并重新验证固件构建。

## 主要业务代码

- `HARDWARE/APP/`：设备主业务状态、应用层调度。
- `HARDWARE/L610/`：4G 模组、MQTT 链路和 AT 指令封装。
- `HARDWARE/LCD/`：本地 LCD 页面、触摸和背光相关代码。
- `HARDWARE/WEATHER/`：风速/风向、雨滴、CJ702 空气质量、Modbus/串口传感器。
- `backend/app/`：FastAPI、SQLAlchemy 模型、MQTT bridge、业务服务和 API schema。
- `qt-client/src/`：Qt C++ 数据层、鉴权、同步、缓存、串口和状态模型。
- `qt-client/qml/`：桌面端和 Android 共用 QML 页面与组件。

## 部署和脚本

- `deploy/linux/`：Jetson/Linux 部署样例，包括 systemd、Caddy 和 MySQL 备份。
- `scripts/`：Windows 下 Qt 环境检查、桌面启动、Android 打包和签名辅助脚本。
- `backend/alembic/`：后端数据库迁移，生产环境优先使用迁移而不是运行时改表。

## 文档

- `README.md`：仓库总入口。
- `docs/README.md`：文档导航。
- `docs/jetson_edge_master_guide_zh.md`：Jetson 现场部署和验收。
- `docs/navicat_mysql_setup.md`：Navicat/MySQL 配置。
- `docs/h743_l610_qt_quick_start.md`：硬件和 Qt 联调。
- `docs/public_remote_deployment_guide.md`：公网访问和 Cloudflare Tunnel 说明。

## 不应提交的内容

这些内容应该留在本机，不进入 Git：

- `backend/.venv/`、`__pycache__/`、`.pytest_cache/`
- `qt-client/build/`、APK/AAB、EXE/DLL/SO 等构建产物
- Keil `Objects/`、`Listings/`、`.hex`、`.map`、`.uvoptx`
- `.env`、证书、私钥、Android keystore
- 本地日志、数据库、临时文件

相关规则见根目录 `.gitignore`、`.gitattributes` 和 `.editorconfig`。
