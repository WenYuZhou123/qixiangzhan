# Cloudflare Tunnel 公网访问部署说明

当前项目公网访问采用 Cloudflare Tunnel：Jetson 或现场 Linux 主机主动连到 Cloudflare，再由 Cloudflare 提供 `https://qixiangzhan.online` 对外访问。腾讯云公网 80/443 直连不再作为验收条件。

## 目标链路

```text
Qt Desktop / Android / Browser
        |
        | HTTPS / WSS
        v
Cloudflare
        |
        | cloudflared tunnel
        v
Jetson / Linux host
        |
        v
FastAPI backend
```

正式 API：

```text
https://qixiangzhan.online/api/v1
```

实时 WebSocket：

```text
wss://qixiangzhan.online/api/v1/ws/realtime
```

## 后端环境变量

生产环境至少配置：

```text
QXZ_API_HOST=127.0.0.1
QXZ_API_PORT=8000
QXZ_PUBLIC_API_BASE=https://qixiangzhan.online/api/v1
QXZ_DATABASE_URL=mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/qixiangzhan?charset=utf8mb4
QXZ_PROJECT_DATABASE_URL=mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/weather?charset=utf8mb4
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

## Cloudflare Tunnel 路由

在 Cloudflare Zero Trust 的 Tunnel Public Hostname 中添加：

```text
Hostname: qixiangzhan.online
Service:  http://127.0.0.1:8000
```

如果需要 `www`：

```text
Hostname: www.qixiangzhan.online
Service:  http://127.0.0.1:8000
```

如果 Jetson 本机先由 Caddy 转发到 FastAPI，则 Service 改成 Caddy 的本机监听地址。

## 客户端配置

Qt 桌面端和 Android App 都使用同一个 API Base：

```text
https://qixiangzhan.online/api/v1
```

Android 不直接连接 MQTT、MySQL 或串口。桌面端工程模式可以保留串口诊断和本地 MQTT 调试。

## 验收顺序

1. Jetson 本机验证：

```bash
curl -i http://127.0.0.1:8000/healthz
```

2. 公网验证：

```bash
curl -i https://qixiangzhan.online/healthz
curl -i https://qixiangzhan.online/openapi.json
```

3. 业务验证：

- `POST /api/v1/auth/login` 登录成功。
- `GET /api/v1/devices` 返回设备列表。
- `WS /api/v1/ws/realtime` 能收到实时消息或 heartbeat。
- Android 4G 网络可登录。
- 外网 Windows 桌面端可登录。
- 命令下发后 `command_messages` 增长并收到 ACK。
- 设备上报后 `telemetry_messages`、`weather_observations`、`weather.project` 持续增长。

## 安全原则

- 不对公网开放 MySQL `3306`。
- 不把 MQTT 管理账号下发给普通客户端。
- `.env`、Token secret、证书、keystore 不提交到 Git。
- Cloudflare Tunnel 可用时，不再依赖云服务器公网 80/443。
