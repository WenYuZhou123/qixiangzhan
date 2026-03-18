#!/usr/bin/env bash
set -euo pipefail

BACKUP_ROOT="/opt/qixiangzhan/backups"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
RETENTION_DAYS=7

: "${QXZ_MYSQL_HOST:=127.0.0.1}"
: "${QXZ_MYSQL_PORT:=3306}"
: "${QXZ_MYSQL_USER:=qixiang_app}"
: "${QXZ_MYSQL_PASSWORD:=change-me}"
: "${QXZ_MYSQL_DATABASE:=qixiangzhan}"

mkdir -p "${BACKUP_ROOT}"

mysqldump \
  --host="${QXZ_MYSQL_HOST}" \
  --port="${QXZ_MYSQL_PORT}" \
  --user="${QXZ_MYSQL_USER}" \
  --password="${QXZ_MYSQL_PASSWORD}" \
  --single-transaction \
  --quick \
  "${QXZ_MYSQL_DATABASE}" \
  | gzip > "${BACKUP_ROOT}/${QXZ_MYSQL_DATABASE}-${STAMP}.sql.gz"

find "${BACKUP_ROOT}" -type f -name "${QXZ_MYSQL_DATABASE}-*.sql.gz" -mtime +${RETENTION_DAYS} -delete
