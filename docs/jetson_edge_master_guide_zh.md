# Jetson 边缘主站现场部署手册

## 目标

Jetson 作为现场边缘主站运行 FastAPI、MySQL、MQTT bridge 和本地看板，服务 1-3 台气象站设备。本地保留 90 天热数据，长期历史后续再同步到云端或外置存储。

## 部署步骤

1. 安装 Python、MySQL Server、MySQL client、Caddy 和系统时钟同步服务。
2. 将 `backend/`、`deploy/linux/`、`scripts/` 拷贝到 `/opt/qixiangzhan/`。
3. 创建 `/opt/qixiangzhan/backend/.env`，至少配置 `QXZ_DATABASE_URL`、`QXZ_MQTT_*`、`QXZ_TOKEN_SECRET`。
4. 在 `backend/` 下创建虚拟环境并安装依赖：

```bash
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
alembic -c alembic.ini upgrade head
```

5. 安装 `deploy/linux/qixiangzhan-backend.service` 到 `/etc/systemd/system/` 并启用。
6. 安装 `deploy/linux/Caddyfile`，只开放 80/443；MySQL 3306 不对公网开放。
7. 配置每日 MySQL 备份：

```bash
0 3 * * * /opt/qixiangzhan/deploy/linux/backup_mysql.sh
```

## 验收

- `curl http://127.0.0.1:8000/healthz` 返回 `ok`。
- 登录后访问 `/api/v1/system/health`，确认 MySQL、MQTT、磁盘空间和设备心跳正常。
- 设备上报后，`devices`、`telemetry_messages`、`weather_observations` 均有数据。
- 断网后本地 API 仍可访问；恢复网络后 MQTT bridge 自动重连。
- 断电重启后 `systemctl status qixiangzhan-backend` 为 running。

## 排障

- 后端启动失败：检查 `.env`、MySQL 用户权限和 `journalctl -u qixiangzhan-backend -f`。
- MQTT 不在线：检查 `QXZ_MQTT_HOST`、TLS、用户名密码和 Jetson 出网。
- 数据不入库：查看 `/api/v1/system/health` 和 `system_events`。
- 磁盘不足：导出备份后清理旧日志、构建目录和超过 90 天的历史数据。
