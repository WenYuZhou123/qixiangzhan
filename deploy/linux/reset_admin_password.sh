#!/usr/bin/env bash
set -euo pipefail

new_password="${1:?Usage: reset_admin_password.sh <new-password>}"
credential_file="/opt/qixiangzhan/INITIAL_ADMIN_CREDENTIALS.txt"

docker exec -i -e QXZ_NEW_ADMIN_PASSWORD="${new_password}" qixiangzhan-backend python - <<'PY'
import os

from app.auth import hash_password, utcnow
from app.database import SessionLocal
from app.models import User

with SessionLocal() as session:
    user = session.query(User).filter(User.username == "admin").one_or_none()
    if user is None:
        raise SystemExit("admin user does not exist")
    user.password_hash = hash_password(os.environ["QXZ_NEW_ADMIN_PASSWORD"])
    user.password_changed_at = utcnow()
    session.commit()
PY

umask 077
printf 'Initial platform account\nUsername: admin\nPassword: %s\n' "${new_password}" > "${credential_file}"
chmod 600 "${credential_file}"
echo 'Admin password updated.'
