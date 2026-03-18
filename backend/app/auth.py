import base64
import hashlib
import hmac
import secrets
from datetime import datetime, timedelta, timezone

import jwt
from fastapi import Depends, HTTPException, WebSocket
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from sqlalchemy import select
from sqlalchemy.orm import Session

from .config import settings
from .database import get_db
from .models import RefreshToken, User

bearer_scheme = HTTPBearer(auto_error=False)
PASSWORD_ITERATIONS = 240000


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


def naive_utc(value: datetime) -> datetime:
    return value.astimezone(timezone.utc).replace(tzinfo=None) if value.tzinfo is not None else value


def _legacy_sha256_password(raw_password: str) -> str:
    return hashlib.sha256(raw_password.encode("utf-8")).hexdigest()


def hash_password(raw_password: str) -> str:
    salt = secrets.token_bytes(16)
    digest = hashlib.pbkdf2_hmac("sha256", raw_password.encode("utf-8"), salt, PASSWORD_ITERATIONS)
    return "pbkdf2_sha256${}${}${}".format(
        PASSWORD_ITERATIONS,
        base64.urlsafe_b64encode(salt).decode("ascii"),
        base64.urlsafe_b64encode(digest).decode("ascii"),
    )


def verify_password(raw_password: str, password_hash: str) -> bool:
    if password_hash.startswith("pbkdf2_sha256$"):
        try:
            _, iteration_text, salt_text, digest_text = password_hash.split("$", 3)
            iterations = int(iteration_text)
            salt = base64.urlsafe_b64decode(salt_text.encode("ascii"))
            expected = base64.urlsafe_b64decode(digest_text.encode("ascii"))
        except Exception:
            return False
        digest = hashlib.pbkdf2_hmac("sha256", raw_password.encode("utf-8"), salt, iterations)
        return hmac.compare_digest(digest, expected)

    return hmac.compare_digest(password_hash, _legacy_sha256_password(raw_password))


def token_hash(token: str) -> str:
    return hashlib.sha256(token.encode("utf-8")).hexdigest()


def _access_token_payload(user: User, now: datetime | None = None) -> tuple[dict, datetime]:
    issued_at = now or utcnow()
    expires_at = issued_at + timedelta(minutes=settings.access_token_minutes)
    payload = {
        "sub": user.username,
        "uid": user.id,
        "role": user.role,
        "type": "access",
        "iss": settings.token_issuer,
        "iat": int(issued_at.timestamp()),
        "exp": int(expires_at.timestamp()),
    }
    return payload, expires_at


def build_access_token(user: User, now: datetime | None = None) -> tuple[str, datetime]:
    payload, expires_at = _access_token_payload(user, now)
    token = jwt.encode(payload, settings.token_secret, algorithm="HS256")
    return token, expires_at


def decode_access_token(token: str) -> dict:
    try:
        payload = jwt.decode(
            token,
            settings.token_secret,
            algorithms=["HS256"],
            issuer=settings.token_issuer,
            options={"require": ["exp", "iat", "iss", "sub", "type"]},
        )
    except jwt.PyJWTError as exc:
        raise HTTPException(status_code=401, detail="Invalid or expired bearer token") from exc

    if payload.get("type") != "access":
        raise HTTPException(status_code=401, detail="Invalid or expired bearer token")
    return payload


def issue_refresh_token(
    db: Session,
    user: User,
    *,
    label: str = "",
    now: datetime | None = None,
) -> tuple[str, RefreshToken, datetime]:
    issued_at = now or utcnow()
    expires_at = issued_at + timedelta(days=settings.refresh_token_days)
    raw_token = secrets.token_urlsafe(48)
    row = RefreshToken(
        user_id=user.id,
        token_hash=token_hash(raw_token),
        label=label,
        expires_at=expires_at,
        created_at=issued_at,
        last_used_at=issued_at,
    )
    db.add(row)
    db.flush()
    return raw_token, row, expires_at


def resolve_refresh_token(db: Session, raw_refresh_token: str) -> RefreshToken | None:
    if not raw_refresh_token:
        return None

    row = db.scalar(select(RefreshToken).where(RefreshToken.token_hash == token_hash(raw_refresh_token)))
    if row is None:
        return None
    if row.revoked_at is not None:
        return None
    if naive_utc(row.expires_at) <= naive_utc(utcnow()):
        return None
    return row


def revoke_refresh_token(db: Session, raw_refresh_token: str) -> bool:
    row = resolve_refresh_token(db, raw_refresh_token)
    if row is None:
        return False
    row.revoked_at = utcnow()
    db.commit()
    return True


def revoke_user_refresh_tokens(db: Session, user_id: int, *, commit: bool = False) -> None:
    rows = db.scalars(
        select(RefreshToken).where(RefreshToken.user_id == user_id, RefreshToken.revoked_at.is_(None))
    ).all()
    now = utcnow()
    for row in rows:
        row.revoked_at = now
    if commit:
        db.commit()


def seed_admin_user(db: Session) -> None:
    admin = db.scalar(select(User).where(User.username == settings.admin_username))
    password_hash = hash_password(settings.admin_password)

    if admin is None:
        admin = User(
            username=settings.admin_username,
            display_name=settings.admin_display_name,
            password_hash=password_hash,
            role="admin",
            is_active=True,
            password_changed_at=utcnow(),
        )
        db.add(admin)
        db.commit()
        return

    admin.display_name = settings.admin_display_name
    admin.password_hash = password_hash
    admin.role = "admin"
    admin.is_active = True
    admin.password_changed_at = utcnow()
    db.commit()


def authenticate(db: Session, username: str, password: str) -> User | None:
    user = db.scalar(select(User).where(User.username == username, User.is_active.is_(True)))
    if user is None:
        return None

    if not verify_password(password, user.password_hash):
        return None

    if not user.password_hash.startswith("pbkdf2_sha256$"):
        user.password_hash = hash_password(password)
        user.password_changed_at = utcnow()
        db.commit()
        db.refresh(user)

    return user


def refresh_session(
    db: Session,
    raw_refresh_token: str,
    *,
    label: str = "",
) -> tuple[User, str, str, datetime, datetime]:
    row = resolve_refresh_token(db, raw_refresh_token)
    if row is None:
        raise HTTPException(status_code=401, detail="Invalid or expired refresh token")

    user = db.scalar(select(User).where(User.id == row.user_id, User.is_active.is_(True)))
    if user is None:
        raise HTTPException(status_code=401, detail="User is inactive")

    row.revoked_at = utcnow()
    access_token, access_expires_at = build_access_token(user)
    new_refresh_token, _, refresh_expires_at = issue_refresh_token(db, user, label=label)
    db.commit()
    return user, access_token, new_refresh_token, access_expires_at, refresh_expires_at


def get_current_user(
    credentials: HTTPAuthorizationCredentials | None = Depends(bearer_scheme),
    db: Session = Depends(get_db),
) -> User:
    token = credentials.credentials if credentials is not None else ""
    if not token:
        raise HTTPException(status_code=401, detail="Invalid or missing bearer token")

    payload = decode_access_token(token)
    user = db.scalar(select(User).where(User.id == payload.get("uid"), User.is_active.is_(True)))
    if user is None:
        raise HTTPException(status_code=401, detail="Invalid or missing bearer token")
    return user


def get_current_user_from_websocket(websocket: WebSocket, db: Session) -> User:
    auth_header = websocket.headers.get("Authorization", "")
    if not auth_header.startswith("Bearer "):
        raise HTTPException(status_code=401, detail="Invalid or missing bearer token")
    token = auth_header[len("Bearer ") :].strip()
    payload = decode_access_token(token)
    user = db.scalar(select(User).where(User.id == payload.get("uid"), User.is_active.is_(True)))
    if user is None:
        raise HTTPException(status_code=401, detail="Invalid or missing bearer token")
    return user
