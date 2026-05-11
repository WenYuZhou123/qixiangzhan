# Public Remote Deployment

This folder contains a production-oriented Linux deployment skeleton for the remote-access version of the platform.
It also fits the Jetson edge-master setup used for on-site deployment.

## Files

- `Caddyfile`: HTTPS reverse proxy sample for `api.qixiangzhan.online`
- `qixiangzhan-backend.service`: systemd service for FastAPI, with Alembic migration before start
- `backup_mysql.sh`: daily MySQL backup script with 7-day retention
- `../backend/alembic/`: versioned database migrations

## Recommended layout

```text
/opt/qixiangzhan/
  backend/
    .venv/
    app/
    .env
  backups/
```

## Deployment steps

1. Copy `backend/` to the Linux host.
2. Create `/opt/qixiangzhan/backend/.env` with production secrets.
3. Install Python, MySQL client tools, and Caddy.
4. Copy `qixiangzhan-backend.service` to `/etc/systemd/system/`.
5. Add an `A` record so `api.qixiangzhan.online` points to the cloud server public IP.
6. Copy `Caddyfile` to `/etc/caddy/Caddyfile`.
7. Keep only `80/443` open on the firewall. Do not expose MySQL `3306`.
8. Run database migrations:

```bash
cd /opt/qixiangzhan/backend
alembic -c alembic.ini upgrade head
```

9. Enable and start the backend and Caddy:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now qixiangzhan-backend
sudo systemctl enable --now caddy
```

10. Add a daily cron entry for backups:

```bash
0 3 * * * /opt/qixiangzhan/deploy/linux/backup_mysql.sh
```

## Health checks

- `GET /healthz` for a quick liveness probe
- `GET /api/v1/system/health` for API, MySQL, MQTT, disk and retention status
- Keep an eye on `system_events` for startup, MQTT reconnect and cleanup runs

## Navicat

Do not expose MySQL `3306` publicly. Use SSH tunnel from Navicat to the Linux host.
