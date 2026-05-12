# Linux / Jetson 部署包

这个目录放 Jetson 或 Linux 主机部署后端时需要的样例文件。当前公网入口采用 Cloudflare Tunnel，腾讯云 80/443 直连不再作为验收条件。

## 文件

- `qixiangzhan-backend.service`：FastAPI 的 systemd 服务，启动前执行 Alembic 迁移
- `backup_mysql.sh`：MySQL 备份脚本
- `Caddyfile`：本机反向代理样例，适合本地 HTTP/HTTPS 或 tunnel 前置代理

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

## Jetson 现网口径

当前线上验收通过的关键事实：

- systemd 服务名：`qixiangzhan-backend`
- 监听地址：`127.0.0.1:8000`
- 对外入口：`https://qixiangzhan.online`
- `www` 子域也通过 Cloudflare Tunnel 指向同一后端
- 主库：`qixiangzhan`
- 兼容库：`weather`

## 环境变量模板

参考 [backend/.env.example](../../backend/.env.example)。

重点值：

```text
QXZ_API_HOST=127.0.0.1
QXZ_API_PORT=8000
QXZ_PUBLIC_API_BASE=https://qixiangzhan.online/api/v1
QXZ_DATABASE_URL=mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/qixiangzhan?charset=utf8mb4
QXZ_PROJECT_DATABASE_URL=mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/weather?charset=utf8mb4
```

注意：

- 仓库只保留脱敏模板
- Jetson 上真实密码、token、密钥只留在 `/opt/qixiangzhan/backend/.env`

## 部署步骤

1. 把 `backend/` 和 `deploy/linux/` 放到 `/opt/qixiangzhan/`
2. 创建 `/opt/qixiangzhan/backend/.env`
3. 安装 Python、MySQL client、`cloudflared`
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

6. 配置 Cloudflare Tunnel：

```text
qixiangzhan.online -> http://127.0.0.1:8000
www.qixiangzhan.online -> http://127.0.0.1:8000
```

## 健康检查

Jetson 本机：

```bash
curl -i http://127.0.0.1:8000/
curl -i http://127.0.0.1:8000/healthz
```

公网：

```bash
curl -i https://qixiangzhan.online/
curl -i https://qixiangzhan.online/healthz
curl -i https://qixiangzhan.online/openapi.json
```

登录后：

```text
GET /api/v1/system/health
GET /api/v1/devices
GET /api/v1/project/latest
```

## Navicat

不要把 MySQL `3306` 直接暴露到公网。Navicat 建议通过 SSH Tunnel 或内网方式连接 Jetson。
