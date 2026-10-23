#!/bin/sh
set -eu

if [ "$(id -u)" -eq 0 ]; then
    install -d -o qixiangzhan -g qixiangzhan /data
    exec su -s /bin/sh qixiangzhan -c 'exec python -m uvicorn app.main:app --host 0.0.0.0 --port 8000'
fi

exec python -m uvicorn app.main:app --host 0.0.0.0 --port 8000
