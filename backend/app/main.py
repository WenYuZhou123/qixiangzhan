import asyncio
import logging
from contextlib import asynccontextmanager

from fastapi import Depends, FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect
from sqlalchemy import func, select, text
from sqlalchemy.orm import Session

from .auth import (
    authenticate,
    build_access_token,
    get_current_user,
    get_current_user_from_websocket,
    hash_password,
    issue_refresh_token,
    refresh_session,
    revoke_refresh_token,
    revoke_user_refresh_tokens,
    seed_admin_user,
    utcnow,
)
from .database import SessionLocal, get_db
from .models import Alarm, CommandMessage, Device, DeviceMembership, TelemetryMessage, User
from .mqtt_bridge import EmqxBridge
from .realtime import realtime_hub
from .schema_migrations import ensure_runtime_schema
from .schemas import (
    AlarmResponse,
    CommandCreateRequest,
    CommandCreateResponse,
    CommandResponse,
    DeviceMembershipRequest,
    DeviceResponse,
    HistoryResponse,
    LoginRequest,
    LogoutRequest,
    MeResponse,
    RefreshRequest,
    SessionResponse,
    UserCreateRequest,
    UserResponse,
    UserUpdateRequest,
)
from .services import (
    allowed_device_ids_for_user,
    apply_runtime_rules,
    create_command_request,
    device_alarm_count_subquery,
    device_ids_for_user,
    replace_user_memberships,
    retry_pending_command_requests,
    serialize_device,
    serialize_user,
    user_can_access_device,
    write_audit_log,
)

logger = logging.getLogger(__name__)
bridge = EmqxBridge()


def request_meta(request: Request) -> tuple[str, str]:
    remote_addr = request.client.host if request.client is not None else ""
    user_agent = request.headers.get("user-agent", "")
    return remote_addr, user_agent


def require_admin(user: User) -> None:
    if user.role != "admin":
        raise HTTPException(status_code=403, detail="Admin role required")


def ensure_device_access(db: Session, user: User, device_id: str) -> None:
    if user_can_access_device(db, user, device_id):
        return
    raise HTTPException(status_code=403, detail="Device access denied")


@asynccontextmanager
async def lifespan(app: FastAPI):
    stop_event = asyncio.Event()
    retry_tracker: dict[str, tuple[int, object]] = {}

    async def command_retry_worker() -> None:
        while not stop_event.is_set():
            try:
                with SessionLocal() as db:
                    retry_pending_command_requests(
                        db,
                        publish=bridge.publish,
                        retry_tracker=retry_tracker,
                    )
            except Exception as exc:  # pragma: no cover
                logger.warning("Pending command retry loop failed: %s", exc)

            try:
                await asyncio.wait_for(stop_event.wait(), timeout=1.0)
            except asyncio.TimeoutError:
                continue

    ensure_runtime_schema()
    with SessionLocal() as db:
        seed_admin_user(db)

    realtime_hub.attach_loop(asyncio.get_running_loop())
    retry_task = asyncio.create_task(command_retry_worker())

    try:
        bridge.start()
    except Exception as exc:  # pragma: no cover
        logger.warning("MQTT bridge start failed: %s", exc)

    try:
        yield
    finally:
        stop_event.set()
        retry_task.cancel()
        try:
            await retry_task
        except asyncio.CancelledError:
            pass
        try:
            bridge.stop()
        except Exception as exc:  # pragma: no cover
            logger.warning("MQTT bridge stop failed: %s", exc)


app = FastAPI(title="qixiangzhan backend", version="0.2.0", lifespan=lifespan)


@app.get("/healthz")
def healthz(db: Session = Depends(get_db)) -> dict[str, object]:
    db.execute(text("SELECT 1"))
    return {
        "status": "ok",
        "time": utcnow().isoformat(),
        "mqtt_bridge_connected": bridge.connected,
    }


@app.post("/api/v1/auth/login", response_model=SessionResponse)
def login(payload: LoginRequest, request: Request, db: Session = Depends(get_db)) -> SessionResponse:
    user = authenticate(db, payload.username, payload.password)
    if user is None:
        raise HTTPException(status_code=401, detail="Invalid credentials")

    user.last_login_at = utcnow()
    access_token, access_expires_at = build_access_token(user)
    refresh_token, _, refresh_expires_at = issue_refresh_token(
        db,
        user,
        label=request.headers.get("user-agent", "")[:120],
    )
    remote_addr, user_agent = request_meta(request)
    write_audit_log(
        db,
        user=user,
        event_type="auth.login",
        message="User logged in",
        remote_addr=remote_addr,
        user_agent=user_agent,
    )
    db.commit()

    return SessionResponse(
        username=user.username,
        display_name=user.display_name,
        role=user.role,
        is_active=user.is_active,
        access_token=access_token,
        refresh_token=refresh_token,
        access_expires_at=access_expires_at,
        refresh_expires_at=refresh_expires_at,
    )


@app.post("/api/v1/auth/refresh", response_model=SessionResponse)
def refresh_auth(payload: RefreshRequest, request: Request, db: Session = Depends(get_db)) -> SessionResponse:
    user, access_token, refresh_token, access_expires_at, refresh_expires_at = refresh_session(
        db,
        payload.refresh_token,
        label=request.headers.get("user-agent", "")[:120],
    )
    remote_addr, user_agent = request_meta(request)
    write_audit_log(
        db,
        user=user,
        event_type="auth.refresh",
        message="Access token refreshed",
        remote_addr=remote_addr,
        user_agent=user_agent,
    )
    db.commit()

    return SessionResponse(
        username=user.username,
        display_name=user.display_name,
        role=user.role,
        is_active=user.is_active,
        access_token=access_token,
        refresh_token=refresh_token,
        access_expires_at=access_expires_at,
        refresh_expires_at=refresh_expires_at,
    )


@app.post("/api/v1/auth/logout")
def logout_auth(payload: LogoutRequest, db: Session = Depends(get_db)) -> dict[str, bool]:
    return {"ok": revoke_refresh_token(db, payload.refresh_token)}


@app.get("/api/v1/auth/me", response_model=MeResponse)
def auth_me(current_user: User = Depends(get_current_user)) -> MeResponse:
    return MeResponse.model_validate(current_user)


@app.get("/api/v1/devices", response_model=list[DeviceResponse])
def list_devices(current_user: User = Depends(get_current_user), db: Session = Depends(get_db)) -> list[DeviceResponse]:
    apply_runtime_rules(db)
    alarm_counts = device_alarm_count_subquery()
    stmt = (
        select(Device, func.coalesce(alarm_counts.c.active_alarm_count, 0))
        .outerjoin(alarm_counts, Device.device_id == alarm_counts.c.device_id)
        .order_by(Device.updated_at.desc())
    )
    if current_user.role != "admin":
        stmt = stmt.join(DeviceMembership, DeviceMembership.device_id == Device.device_id).where(
            DeviceMembership.user_id == current_user.id
        )

    rows = db.execute(stmt).all()
    return [DeviceResponse.model_validate(serialize_device(row[0], int(row[1]))) for row in rows]


@app.get("/api/v1/devices/{device_id}", response_model=DeviceResponse)
def get_device(device_id: str, current_user: User = Depends(get_current_user), db: Session = Depends(get_db)) -> DeviceResponse:
    apply_runtime_rules(db)
    ensure_device_access(db, current_user, device_id)
    alarm_count = db.scalar(
        select(func.count(Alarm.id)).where(Alarm.device_id == device_id, Alarm.active.is_(True))
    ) or 0
    device = db.scalar(select(Device).where(Device.device_id == device_id))
    if device is None:
        raise HTTPException(status_code=404, detail="Device not found")
    return DeviceResponse.model_validate(serialize_device(device, int(alarm_count)))


@app.get("/api/v1/devices/{device_id}/history", response_model=list[HistoryResponse])
def get_device_history(
    device_id: str,
    limit: int = 200,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> list[HistoryResponse]:
    apply_runtime_rules(db)
    ensure_device_access(db, current_user, device_id)
    rows = db.scalars(
        select(TelemetryMessage)
        .where(TelemetryMessage.device_id == device_id)
        .order_by(TelemetryMessage.created_at.desc())
        .limit(limit)
    ).all()
    return [HistoryResponse.model_validate(row) for row in rows]


@app.get("/api/v1/devices/{device_id}/alarms", response_model=list[AlarmResponse])
def get_device_alarms(
    device_id: str,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> list[AlarmResponse]:
    apply_runtime_rules(db)
    ensure_device_access(db, current_user, device_id)
    rows = db.scalars(
        select(Alarm).where(Alarm.device_id == device_id).order_by(Alarm.active.desc(), Alarm.created_at.desc())
    ).all()
    return [AlarmResponse.model_validate(row) for row in rows]


@app.get("/api/v1/commands", response_model=list[CommandResponse])
def list_commands(
    device_id: str | None = None,
    msg_id: str | None = None,
    limit: int = 200,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> list[CommandResponse]:
    apply_runtime_rules(db)
    stmt = select(CommandMessage)
    if device_id:
        ensure_device_access(db, current_user, device_id)
        stmt = stmt.where(CommandMessage.device_id == device_id)
    elif current_user.role != "admin":
        allowed_ids = sorted(allowed_device_ids_for_user(db, current_user))
        if not allowed_ids:
            return []
        stmt = stmt.where(CommandMessage.device_id.in_(allowed_ids))

    if msg_id:
        stmt = stmt.where(CommandMessage.msg_id == msg_id)

    rows = db.scalars(stmt.order_by(CommandMessage.created_at.desc()).limit(limit)).all()
    return [CommandResponse.model_validate(row) for row in rows]


@app.post("/api/v1/devices/{device_id}/commands", response_model=CommandCreateResponse)
def publish_device_command(
    device_id: str,
    payload: CommandCreateRequest,
    request: Request,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> CommandCreateResponse:
    ensure_device_access(db, current_user, device_id)
    requested_device_id = payload.device_id.strip()
    if requested_device_id and requested_device_id != device_id:
        raise HTTPException(status_code=400, detail="device_id path/body mismatch")

    operator_label = f"{current_user.username}:{payload.operator or 'client'}"
    try:
        command_row = create_command_request(
            db,
            device_id=device_id,
            command=payload.command,
            value=payload.value,
            operator=operator_label,
            publish=bridge.publish,
        )
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc

    remote_addr, user_agent = request_meta(request)
    write_audit_log(
        db,
        user=current_user,
        event_type="command.publish",
        message=f"Published {payload.command}",
        device_id=device_id,
        payload={"command": payload.command, "value": payload.value, "msg_id": command_row.msg_id},
        remote_addr=remote_addr,
        user_agent=user_agent,
    )
    db.commit()

    return CommandCreateResponse(
        msg_id=command_row.msg_id or "",
        device_id=command_row.device_id,
        command=command_row.command,
        status=command_row.status,
        created_at=command_row.created_at,
    )


@app.get("/api/v1/users", response_model=list[UserResponse])
def list_users(current_user: User = Depends(get_current_user), db: Session = Depends(get_db)) -> list[UserResponse]:
    require_admin(current_user)
    rows = db.scalars(select(User).order_by(User.created_at.asc(), User.username.asc())).all()
    return [UserResponse.model_validate(serialize_user(row, device_ids_for_user(db, row.id))) for row in rows]


@app.post("/api/v1/users", response_model=UserResponse)
def create_user(
    payload: UserCreateRequest,
    request: Request,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> UserResponse:
    require_admin(current_user)
    if payload.role not in {"admin", "operator"}:
        raise HTTPException(status_code=400, detail="Unsupported role")
    if db.scalar(select(User).where(User.username == payload.username.strip())) is not None:
        raise HTTPException(status_code=409, detail="Username already exists")

    user = User(
        username=payload.username.strip(),
        display_name=payload.display_name.strip() or payload.username.strip(),
        password_hash=hash_password(payload.password),
        role=payload.role,
        is_active=payload.is_active,
        password_changed_at=utcnow(),
    )
    db.add(user)
    db.flush()
    replace_user_memberships(db, user.id, payload.device_ids)
    remote_addr, user_agent = request_meta(request)
    write_audit_log(
        db,
        user=current_user,
        event_type="user.create",
        message=f"Created user {user.username}",
        payload={"role": user.role, "device_ids": sorted(payload.device_ids)},
        remote_addr=remote_addr,
        user_agent=user_agent,
    )
    db.commit()
    db.refresh(user)
    return UserResponse.model_validate(serialize_user(user, device_ids_for_user(db, user.id)))


@app.patch("/api/v1/users/{user_id}", response_model=UserResponse)
def update_user(
    user_id: int,
    payload: UserUpdateRequest,
    request: Request,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> UserResponse:
    require_admin(current_user)
    user = db.scalar(select(User).where(User.id == user_id))
    if user is None:
        raise HTTPException(status_code=404, detail="User not found")

    if payload.display_name is not None:
        user.display_name = payload.display_name.strip() or user.username
    if payload.role is not None:
        if payload.role not in {"admin", "operator"}:
            raise HTTPException(status_code=400, detail="Unsupported role")
        user.role = payload.role
    if payload.password:
        user.password_hash = hash_password(payload.password)
        user.password_changed_at = utcnow()
        revoke_user_refresh_tokens(db, user.id)
        db.add(user)
    if payload.is_active is not None:
        user.is_active = payload.is_active
        if not payload.is_active:
            revoke_user_refresh_tokens(db, user.id)
            db.add(user)

    remote_addr, user_agent = request_meta(request)
    write_audit_log(
        db,
        user=current_user,
        event_type="user.update",
        message=f"Updated user {user.username}",
        payload={
            "display_name": payload.display_name,
            "role": payload.role,
            "is_active": payload.is_active,
            "password_changed": bool(payload.password),
        },
        remote_addr=remote_addr,
        user_agent=user_agent,
    )
    db.commit()
    db.refresh(user)
    return UserResponse.model_validate(serialize_user(user, device_ids_for_user(db, user.id)))


@app.post("/api/v1/users/{user_id}/devices", response_model=UserResponse)
def assign_user_devices(
    user_id: int,
    payload: DeviceMembershipRequest,
    request: Request,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> UserResponse:
    require_admin(current_user)
    user = db.scalar(select(User).where(User.id == user_id))
    if user is None:
        raise HTTPException(status_code=404, detail="User not found")
    replace_user_memberships(db, user.id, payload.device_ids)
    remote_addr, user_agent = request_meta(request)
    write_audit_log(
        db,
        user=current_user,
        event_type="user.assign_devices",
        message=f"Assigned devices to {user.username}",
        payload={"device_ids": sorted(payload.device_ids)},
        remote_addr=remote_addr,
        user_agent=user_agent,
    )
    db.commit()
    db.refresh(user)
    return UserResponse.model_validate(serialize_user(user, device_ids_for_user(db, user.id)))


@app.websocket("/api/v1/ws/realtime")
async def realtime_socket(websocket: WebSocket) -> None:
    with SessionLocal() as db:
        try:
            user = get_current_user_from_websocket(websocket, db)
        except HTTPException:
            await websocket.close(code=1008)
            return
        device_ids = allowed_device_ids_for_user(db, user)

    await realtime_hub.register(websocket, user_id=user.id, role=user.role, device_ids=device_ids)
    await websocket.send_json(
        {"type": "heartbeat", "device_id": None, "timestamp": utcnow().isoformat(), "payload": {"status": "connected"}}
    )
    try:
        while True:
            await websocket.receive_text()
            await websocket.send_json(
                {"type": "heartbeat", "device_id": None, "timestamp": utcnow().isoformat(), "payload": {"status": "ok"}}
            )
    except WebSocketDisconnect:
        realtime_hub.unregister(websocket)
