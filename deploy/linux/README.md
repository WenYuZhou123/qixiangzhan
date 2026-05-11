# Linux / Jetson 部署包

这个目录放 Jetson 或 Linux 主机部署后端时需要的样例文件。当前公网入口采用 Cloudflare Tunnel，腾讯云 80/443 直连不再作为验收条件。

## 文件

- `qixiangzhan-backend.service`：FastAPI systemd 服务，启动前执行 Alembic 迁移。
- `backup_mysql.sh`：MySQL 每日备份脚本，默认保留 7 天。
- `Caddyfile`：本机反向代理样例，可用于 Jetson 本地 HTTP/HTTPS 入口或 tunnel 前置代理。
- `../../backend/alembic/`：后端数据库版本迁移。

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
