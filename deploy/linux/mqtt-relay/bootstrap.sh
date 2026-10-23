#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$script_dir"

if [ ! -f .env ]; then
    echo "Missing .env. Copy .env.example to .env and set L610_RELAY_PASSWORD." >&2
    exit 1
fi

# shellcheck disable=SC1091
. ./.env

if [ -z "${L610_RELAY_PASSWORD:-}" ]; then
    echo "L610_RELAY_PASSWORD is required." >&2
    exit 1
fi

docker run --rm --user 0:0 -v "$script_dir:/work" eclipse-mosquitto:2 \
    sh -c 'rm -f /work/passwordfile && mosquitto_passwd -b -c /work/passwordfile l610_relay_001 "$1" && chown 1883:1883 /work/passwordfile /work/aclfile && chmod 600 /work/passwordfile /work/aclfile' \
    sh "$L610_RELAY_PASSWORD"

mkdir -p data log
docker run --rm --user 0:0 \
    -v "$script_dir/data:/work/data" \
    -v "$script_dir/log:/work/log" \
    eclipse-mosquitto:2 \
    sh -c 'chown -R 1883:1883 /work/data /work/log'
docker compose up -d
docker compose ps
