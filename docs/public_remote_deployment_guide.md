# 公网异地访问部署指南

适用于当前 `Qt Desktop + Qt Android + FastAPI + MySQL + EMQX` 项目，目标是让外网 Windows 电脑和 Android 手机都能直接访问。

## 1. 最终拓扑

- 设备 -> `EMQX` 公网实例
- FastAPI 后端 -> `EMQX` TLS
- Android / Windows 客户端 -> `HTTPS API + WebSocket`
- MySQL 仅内网或本机可见，不开放 `3306`
- Navicat 通过 `SSH Tunnel` 管理数据库

## 2. 必备环境变量

生产环境至少配置：

```text
QXZ_API_HOST=0.0.0.0
QXZ_API_PORT=8000
QXZ_PUBLIC_API_BASE=https://api.qixiangzhan.online/api/v1
QXZ_DATABASE_URL=mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/qixiangzhan?charset=utf8mb4
QXZ_MQTT_HOST=rc11adc1.ala.cn-hangzhou.emqxsl.cn
QXZ_MQTT_PORT=8883
QXZ_MQTT_USERNAME=h743
QXZ_MQTT_PASSWORD=123456
QXZ_MQTT_USE_TLS=true
QXZ_TOKEN_SECRET=replace-with-a-long-random-secret
QXZ_TOKEN_ISSUER=qixiangzhan-backend
QXZ_ACCESS_TOKEN_MINUTES=15
QXZ_REFRESH_TOKEN_DAYS=30
QXZ_JSON_LOGS=true
```

## 3. 现在已经支持的公网能力

- Access Token + Refresh Token
- `admin / operator` 角色
- 设备授权关系
- `GET /api/v1/auth/me`
- `POST /api/v1/auth/refresh`
- `POST /api/v1/auth/logout`
- `GET/POST/PATCH /api/v1/users*`
- `WS /api/v1/ws/realtime`
- `GET /healthz`

## 4. 桌面端和安卓端

- 桌面端默认建议使用“远程模式”
- 串口诊断和直连 MQTT 仅保留给 Windows 工程模式
- Android 不直连 MQTT，只走 API + WebSocket
- 如果当前 Qt 缺少 `Qt6 WebSockets`，客户端会自动退回到 REST 轮询

## 5. 上云后你要改的地方

- 登录页 `API Base` 改成公网域名，例如：
  - `https://api.qixiangzhan.online/api/v1`
- Android 设置页也改为同一公网地址
- 不再把 MySQL 暴露给公网
- 不再把 EMQX 用户名密码下发到普通客户端

## 6. 域名与防火墙

- 为 `api.qixiangzhan.online` 添加 `A` 记录，指向云服务器公网 IP
- 服务器只开放 `80` 和 `443`
- `3306` 仅允许本机或内网访问，不对公网开放
- 由 Caddy 自动申请并续期 HTTPS 证书

## 7. 验证顺序

1. 云主机上 `curl https://api.qixiangzhan.online/healthz`
2. `POST /api/v1/auth/login`
3. `GET /api/v1/auth/me`
4. `WS /api/v1/ws/realtime` 收到 `heartbeat`
5. Android 4G 网络登录成功
6. 外网 Windows 电脑登录成功
7. 下发命令后 `command_messages` 增长
8. 设备上报后 `telemetry_messages` 增长
