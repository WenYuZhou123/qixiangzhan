#!/usr/bin/env bash
set -euo pipefail

api_base="${1:-https://api.qixiangzhan.online}"
credential_file="${2:-/opt/qixiangzhan/INITIAL_ADMIN_CREDENTIALS.txt}"

password="$(sed -n 's/^Password: //p' "${credential_file}")"
test -n "${password}"

health="$(curl --insecure --fail --silent --show-error "${api_base}/healthz")"
login="$(curl --insecure --fail --silent --show-error \
    -X POST \
    -H 'Content-Type: application/json' \
    --data "{\"username\":\"admin\",\"password\":\"${password}\"}" \
    "${api_base}/api/v1/auth/login")"
token="$(printf '%s' "${login}" | python3 -c 'import json, sys; print(json.load(sys.stdin)["access_token"])')"

me_status="$(curl --insecure --silent --output /dev/null --write-out '%{http_code}' \
    -H "Authorization: Bearer ${token}" \
    "${api_base}/api/v1/auth/me")"
device_response_file="$(mktemp)"
trap 'rm -f "${device_response_file}"' EXIT
device_status="$(curl --insecure --silent --output "${device_response_file}" --write-out '%{http_code}' \
    -H "Authorization: Bearer ${token}" \
    "${api_base}/api/v1/devices")"

test "${me_status}" = "200"
test "${device_status}" = "200"

printf 'health=%s\n' "$(printf '%s' "${health}" | python3 -c 'import json, sys; print(json.load(sys.stdin)["status"])')"
printf 'auth_me=%s\n' "${me_status}"
printf 'devices=%s\n' "${device_status}"
printf 'device_count=%s\n' "$(python3 -c 'import json, sys; print(len(json.load(sys.stdin)))' < "${device_response_file}")"

printf '%s' "${token}" | docker exec -i qixiangzhan-backend python -c '
import asyncio
import ssl
import sys
import websockets

token = sys.stdin.read().strip()

async def check():
    async with websockets.connect(
        "wss://api.qixiangzhan.online/api/v1/ws/realtime",
        additional_headers={"Authorization": f"Bearer {token}"},
        ssl=ssl.create_default_context(),
    ) as socket:
        message = await socket.recv()
        assert "heartbeat" in message

asyncio.run(check())
print("websocket=heartbeat")
'
