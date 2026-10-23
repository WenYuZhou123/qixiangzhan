# Linux / Jetson 部署包

这个目录放 Jetson 或 Linux 主机部署后端时需要的样例文件。当前公网入口采用 Cloudflare Tunnel，腾讯云 80/443 直连不再作为验收条件。
This folder contains production-oriented Linux deployment options for the platform.

## 文件

- `qixiangzhan-backend.service`：FastAPI systemd 服务，启动前执行 Alembic 迁移。
- `backup_mysql.sh`：MySQL 每日备份脚本，默认保留 7 天。
- `Caddyfile`：本机反向代理样例，可用于 Jetson 本地 HTTP/HTTPS 入口或 tunnel 前置代理。
- `../../backend/alembic/`：后端数据库版本迁移。
- `Caddyfile`: HTTPS reverse proxy sample for `api.qixiangzhan.online`
- `qixiangzhan-backend.service`: systemd service for FastAPI
- `backup_mysql.sh`: daily MySQL backup script with 7-day retention
- `docker-compose.shared-proxy.yml`: isolated deployment for a server that already runs the shared Caddy proxy
- `docker-compose.edge.yml`: standalone backend and Caddy deployment for `edge.qixiangzhan.online`

## Shared Caddy proxy deployment

Use this option when the host already has a Caddy container on Docker network
`unitree-server_public`. The backend remains private to that network and stores
its single-instance SQLite database in the `qixiangzhan-data` Docker volume.

1. Copy `backend/` and `deploy/linux/` to `/opt/qixiangzhan/`.
2. Copy `.env.docker.example` to `.env`, then replace the MQTT password,
   administrator password, and token secret with production values.
3. Start the service:

```bash
sudo docker compose -f /opt/qixiangzhan/deploy/linux/docker-compose.shared-proxy.yml up -d --build
```

4. Append `Caddyfile.shared-proxy.snippet` to the shared Caddyfile, validate
   it in the Caddy container, and reload Caddy.

The DNS record for `api.qixiangzhan.online` must resolve to the server before
the public HTTPS endpoint can be verified.

## Standalone edge deployment

Use this option for a standalone edge server. It includes its own Caddy
container and keeps the FastAPI container private. The supplied Compose file
publishes only port 443 and uses the TLS-ALPN ACME challenge, allowing a host
that already reserves port 80 for MQTT to remain unchanged.

1. Add a DNS-only A record: `edge.qixiangzhan.online` to the server public IP.
2. Copy `.env.edge.example` to `.env` and replace every `replace-with-*` value.
3. Start the stack from this directory:

```bash
sudo docker compose -f docker-compose.edge.yml up -d --build
```

4. Verify `https://edge.qixiangzhan.online/healthz` after DNS propagation.

## 推荐目录

```text
/opt/qixiangzhan/
  backend/
    .venv/
    app/
    .env
  deploy/
    linux/
  backups/
```

## Jetson 部署步骤

1. 把 `backend/` 和 `deploy/linux/` 放到 `/opt/qixiangzhan/`。
2. 创建 `/opt/qixiangzhan/backend/.env`，写入 MySQL、MQTT、Token 和公网 API 配置。
3. 安装 Python、MySQL client、Caddy、cloudflared。
4. 执行迁移：

```bash
cd /opt/qixiangzhan/backend
alembic -c alembic.ini upgrade head
```

5. 安装并启动 systemd 服务：

```bash
sudo cp /opt/qixiangzhan/deploy/linux/qixiangzhan-backend.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now qixiangzhan-backend
```

6. 配置 Cloudflare Tunnel，把公网主机名转发到本机后端或 Caddy：

```text
qixiangzhan.online -> http://127.0.0.1:8000
www.qixiangzhan.online -> http://127.0.0.1:8000
```

如果本机使用 Caddy 作为前置代理，则 tunnel service 填 Caddy 监听地址。

## 健康检查

本机：

```bash
curl -i http://127.0.0.1:8000/healthz
```

公网：

```bash
curl -i https://qixiangzhan.online/healthz
curl -i https://qixiangzhan.online/openapi.json
```

系统健康：

```text
GET /api/v1/system/health
```

## Navicat

不要把 MySQL `3306` 暴露到公网。Navicat 推荐通过 SSH Tunnel 或 Cloudflare 内网访问方案连接 Jetson。
