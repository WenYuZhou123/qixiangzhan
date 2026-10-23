#!/usr/bin/env bash
set -euo pipefail

DEPLOY_DIR="${1:-/opt/qixiangzhan/deploy/linux}"
ENV_FILE="${DEPLOY_DIR}/.env"
CREDENTIAL_FILE="/opt/qixiangzhan/INITIAL_ADMIN_CREDENTIALS.txt"

if [[ -e "${ENV_FILE}" ]]; then
    echo "Refusing to overwrite existing ${ENV_FILE}" >&2
    exit 1
fi

token_secret="$(openssl rand -hex 32)"
admin_password="$(openssl rand -hex 20)"

umask 077
cat > "${ENV_FILE}" <<EOF
QXZ_PUBLIC_API_BASE=https://api.qixiangzhan.online/api/v1
QXZ_DATABASE_URL=sqlite:////data/qixiangzhan.db
QXZ_MQTT_HOST=rc11adc1.ala.cn-hangzhou.emqxsl.cn
QXZ_MQTT_PORT=8883
QXZ_MQTT_USERNAME=h743
QXZ_MQTT_PASSWORD=123456
QXZ_MQTT_USE_TLS=true
QXZ_MQTT_KEEPALIVE=60
QXZ_ADMIN_USERNAME=admin
QXZ_ADMIN_PASSWORD=${admin_password}
QXZ_ADMIN_DISPLAY_NAME=Platform Admin
QXZ_TOKEN_SECRET=${token_secret}
QXZ_TOKEN_ISSUER=qixiangzhan-backend
QXZ_ACCESS_TOKEN_MINUTES=15
QXZ_REFRESH_TOKEN_DAYS=30
QXZ_OFFLINE_SECONDS=90
QXZ_ACK_TIMEOUT_SECONDS=15
QXZ_WS_HEARTBEAT_SECONDS=20
QXZ_JSON_LOGS=false
EOF

cat > "${CREDENTIAL_FILE}" <<EOF
Initial platform account
Username: admin
Password: ${admin_password}

Change this password from the platform user management page after first login.
EOF

chmod 600 "${ENV_FILE}" "${CREDENTIAL_FILE}"
echo "Created ${ENV_FILE} and ${CREDENTIAL_FILE}"
