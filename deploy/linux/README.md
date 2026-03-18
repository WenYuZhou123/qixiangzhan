# Public Remote Deployment

This folder contains a production-oriented Linux deployment skeleton for the remote-access version of the platform.

## Files

- `Caddyfile`: HTTPS reverse proxy sample for `api.qixiangzhan.online`
- `qixiangzhan-backend.service`: systemd service for FastAPI
- `backup_mysql.sh`: daily MySQL backup script with 7-day retention

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
8. Enable and start the backend and Caddy:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now qixiangzhan-backend
sudo systemctl enable --now caddy
```

9. Add a daily cron entry for backups:

```bash
0 3 * * * /opt/qixiangzhan/deploy/linux/backup_mysql.sh
```

## Navicat

Do not expose MySQL `3306` publicly. Use SSH tunnel from Navicat to the Linux host.
