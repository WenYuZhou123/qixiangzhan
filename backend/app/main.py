import asyncio
import os
import shutil
import logging
from contextlib import asynccontextmanager
from datetime import datetime

from fastapi import Depends, FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect
from sqlalchemy import func, select, text
from sqlalchemy.orm import Session

from .config import settings
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
from .schemas import (
    AlarmResponse,
    CommandCreateRequest,
    CommandCreateResponse,
    CommandResponse,
    DeviceMembershipRequest,
    DeviceDiagnosticsResponse,
    DeviceResponse,
    HistoryResponse,
    LoginRequest,
    LogoutRequest,
    MeResponse,
    ProjectTelemetryResponse,
    RefreshRequest,
    SessionResponse,
    SystemHealthResponse,
    WeatherObservationResponse,
    UserCreateRequest,
    UserResponse,
    UserUpdateRequest,
)
from .project_storage import (
    ProjectSessionLocal,
    ProjectTelemetry,
    ensure_project_schema,
    load_latest_project_by_device_ids,
    load_latest_project_rows,
    project_database_label,
)
from .services import (
    allowed_device_ids_for_user,
    apply_runtime_rules,
    create_command_request,
    create_system_event,
    device_alarm_count_subquery,
    device_ids_for_user,
    replace_user_memberships,
    prune_runtime_history,
    retry_pending_command_requests,
    serialize_device,
    serialize_user,
    sensor_status_from_payload,
    user_can_access_device,
    write_audit_log,
)
from .models import DeviceLastState, WeatherObservation

logger = logging.getLogger(__name__)
bridge = EmqxBridge()
APP_STARTED_AT = utcnow()


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


def latest_project_map_for_devices(device_ids: list[str]) -> dict[str, ProjectTelemetry]:
    if not device_ids:
        return {}
    try:
        with ProjectSessionLocal() as project_db:
            return load_latest_project_by_device_ids(project_db, device_ids)
    except Exception as exc:  # pragma: no cover - optional external database
        logger.warning("Project database read failed: %s", exc)
        return {}


def project_rows_for_user(
    *,
    db: Session,
    user: User,
    device_id: str | None = None,
    limit: int = 200,
) -> list[ProjectTelemetry]:
    allowed_ids: list[str] | None = None
    if device_id:
        ensure_device_access(db, user, device_id)
    elif user.role != "admin":
        allowed_ids = sorted(allowed_device_ids_for_user(db, user))
        if not allowed_ids:
            return []

    try:
        with ProjectSessionLocal() as project_db:
            return load_latest_project_rows(
                project_db,
                device_id=device_id,
                device_ids=allowed_ids,
                limit=limit,
            )
    except Exception as exc:  # pragma: no cover - optional external database
        logger.warning("Project database read failed: %s", exc)
        return []


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

    async def housekeeping_worker() -> None:
        while not stop_event.is_set():
            try:
                with SessionLocal() as db:
                    prune_runtime_history(db)
            except Exception as exc:  # pragma: no cover
                logger.warning("Runtime housekeeping failed: %s", exc)

            try:
                await asyncio.wait_for(
                    stop_event.wait(),
                    timeout=max(60, settings.retention_cleanup_interval_seconds),
                )
            except asyncio.TimeoutError:
                continue

    with SessionLocal() as db:
        seed_admin_user(db)
        create_system_event(
            db,
            event_type="service.start",
            message="Backend service starting",
            source="backend",
            payload={"retention_days": settings.retention_days},
            commit=True,
        )
        try:
            ensure_project_schema()
        except Exception as exc:  # pragma: no cover - optional external database
            logger.warning("Project database unavailable: %s", exc)
            create_system_event(
                db,
                event_type="project.storage_unavailable",
                severity="warning",
                source="project-db",
                message="weather.project storage is unavailable; main backend startup continues",
                payload={
                    "database_url": project_database_label(),
                    "error": str(exc),
                },
                commit=True,
            )

    realtime_hub.attach_loop(asyncio.get_running_loop())
    retry_task = asyncio.create_task(command_retry_worker())
    housekeeping_task = asyncio.create_task(housekeeping_worker())

    try:
        bridge.start()
    except Exception as exc:  # pragma: no cover
        logger.warning("MQTT bridge start failed: %s", exc)

    try:
        yield
    finally:
        stop_event.set()
        for task in (retry_task, housekeeping_task):
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass
        try:
            bridge.stop()
        except Exception as exc:  # pragma: no cover
            logger.warning("MQTT bridge stop failed: %s", exc)
        try:
            with SessionLocal() as db:
                create_system_event(
                    db,
                    event_type="service.stop",
                    message="Backend service stopped",
                    source="backend",
                    commit=True,
                )
        except Exception as exc:  # pragma: no cover
            logger.warning("Failed to record stop event: %s", exc)


app = FastAPI(title="qixiangzhan backend", version="0.2.0", lifespan=lifespan)


@app.get("/healthz")
def healthz(db: Session = Depends(get_db)) -> dict[str, object]:
    db.execute(text("SELECT 1"))
    return {
        "status": "ok",
        "time": utcnow().isoformat(),
        "mqtt_bridge_connected": bridge.connected,
    }


@app.get("/api/v1/system/health", response_model=SystemHealthResponse)
def system_health(current_user: User = Depends(get_current_user), db: Session = Depends(get_db)) -> SystemHealthResponse:
    _ = current_user
    db.execute(text("SELECT 1"))

    device_count = db.scalar(select(func.count(Device.id))) or 0
    online_count = db.scalar(select(func.count(Device.id)).where(Device.online.is_(True))) or 0
    latest_seen = db.scalar(select(func.max(Device.last_seen_at)))
    latest_obs = db.scalar(select(func.max(WeatherObservation.observed_at)))
    uptime_seconds = max(0.0, (utcnow() - APP_STARTED_AT).total_seconds())
    disk_usage = shutil.disk_usage(os.getcwd())
    disk_free_percent = (disk_usage.free / disk_usage.total * 100.0) if disk_usage.total else 0.0

    status = "ok"
    if not bridge.connected:
        status = "degraded"
    if disk_free_percent < 10.0:
        status = "warning"

    return SystemHealthResponse(
        status=status,
        time=utcnow(),
        uptime_seconds=uptime_seconds,
        api={
            "host": settings.api_host,
            "port": settings.api_port,
            "version": app.version,
            "connected_devices": int(online_count),
            "total_devices": int(device_count),
        },
        mysql={
            "database_url": settings.database_url.rsplit("@", 1)[-1] if "@" in settings.database_url else settings.database_url,
            "ok": True,
        },
        mqtt={
            "connected": bridge.connected,
            "host": settings.mqtt_host,
            "port": settings.mqtt_port,
        },
        disk={
            "total": disk_usage.total,
            "used": disk_usage.used,
            "free": disk_usage.free,
            "free_percent": round(disk_free_percent, 2),
        },
        devices={
            "last_seen_at": latest_seen,
            "last_weather_at": latest_obs,
            "online": int(online_count),
            "total": int(device_count),
        },
        retention={
            "days": settings.retention_days,
            "cleanup_interval_seconds": settings.retention_cleanup_interval_seconds,
        },
    )


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
    device_ids = [row[0].device_id for row in rows]
    latest_project = latest_project_map_for_devices(device_ids)
    return [
        DeviceResponse.model_validate(
            serialize_device(row[0], int(row[1]), project=latest_project.get(row[0].device_id))
        )
        for row in rows
    ]


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
    latest_project = latest_project_map_for_devices([device.device_id])
    return DeviceResponse.model_validate(
        serialize_device(device, int(alarm_count), project=latest_project.get(device.device_id))
    )


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


@app.get("/api/v1/devices/{device_id}/weather/history", response_model=list[WeatherObservationResponse])
def get_device_weather_history(
    device_id: str,
    start: datetime | None = None,
    end: datetime | None = None,
    limit: int = 500,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> list[WeatherObservationResponse]:
    apply_runtime_rules(db)
    ensure_device_access(db, current_user, device_id)
    stmt = select(WeatherObservation).where(WeatherObservation.device_id == device_id)
    if start is not None:
        stmt = stmt.where(WeatherObservation.observed_at >= start)
    if end is not None:
        stmt = stmt.where(WeatherObservation.observed_at <= end)
    rows = db.scalars(stmt.order_by(WeatherObservation.observed_at.desc()).limit(limit)).all()
    return [WeatherObservationResponse.model_validate(row) for row in rows]


@app.get("/api/v1/project/latest", response_model=list[ProjectTelemetryResponse])
def list_latest_project_telemetry(
    device_id: str | None = None,
    limit: int = 200,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> list[ProjectTelemetryResponse]:
    limit = max(1, min(limit, 1000))
    rows = project_rows_for_user(db=db, user=current_user, device_id=device_id, limit=limit)
    return [ProjectTelemetryResponse.model_validate(row) for row in rows]


@app.get("/api/v1/project/{device_id}/history", response_model=list[ProjectTelemetryResponse])
def get_project_device_history(
    device_id: str,
    limit: int = 500,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> list[ProjectTelemetryResponse]:
    limit = max(1, min(limit, 2000))
    rows = project_rows_for_user(db=db, user=current_user, device_id=device_id, limit=limit)
    return [ProjectTelemetryResponse.model_validate(row) for row in rows]


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


@app.get("/api/v1/devices/{device_id}/diagnostics", response_model=DeviceDiagnosticsResponse)
def get_device_diagnostics(
    device_id: str,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> DeviceDiagnosticsResponse:
    apply_runtime_rules(db)
    ensure_device_access(db, current_user, device_id)
    device = db.scalar(select(Device).where(Device.device_id == device_id))
    if device is None:
        raise HTTPException(status_code=404, detail="Device not found")

    last_state = db.scalar(select(DeviceLastState).where(DeviceLastState.device_id == device_id))
    payload = last_state.payload if last_state is not None else {}
    sensor_status = sensor_status_from_payload(device.weather_sensor_status or {})
    if not device.weather_sensor_status:
        sensor_status["wind_online"] = device.weather_capability_wind
        sensor_status["air_online"] = device.weather_capability_air
        sensor_status["rain_online"] = device.weather_capability_rain
    l610 = {
        "state": str(sensor_status.get("l610_state") or device.state_text or ""),
        "rssi": device.rssi,
        "operator": device.operator_name,
        "ip": device.ip,
        "last_status_ok": bool(device.online),
        "last_publish_ok": bool(device.last_seen_at is not None),
    }

    return DeviceDiagnosticsResponse(
        device_id=device.device_id,
        online=device.online,
        last_seen_at=device.last_seen_at,
        updated_at=device.updated_at,
        l610=l610,
        sensor_status=sensor_status,
        last_state=payload,
    )


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
