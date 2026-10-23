import asyncio
import os
import shutil
import hmac
import http.client
import logging
import threading
import time
from contextlib import asynccontextmanager
from datetime import datetime
from pathlib import Path
from urllib.parse import urlsplit, urlunsplit

from fastapi import Depends, FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, Response
from sqlalchemy import func, select, text
from sqlalchemy.orm import Session

from .config import settings
from .auth import (
    authenticate,
    build_access_token,
    get_current_user,
    get_current_user_from_websocket,
    hash_password,
    issue_media_credentials,
    media_ticket_device_id,
    issue_turn_credentials,
    media_ticket_user_id,
    issue_refresh_token,
    refresh_session,
    revoke_refresh_token,
    revoke_user_refresh_tokens,
    seed_admin_user,
    utcnow,
    verify_media_credentials,
)
from .database import SessionLocal, get_db
from .config import settings
from .ai_service import AIProviderError, analyze_with_provider
from .models import AIAnalysis, Alarm, CommandMessage, Device, DeviceMembership, ForecastSnapshot, TelemetryMessage, User
from .mqtt_bridge import EmqxBridge
from .realtime import realtime_hub
from .schema_migrations import ensure_runtime_schema
from .weather_forecast import ForecastProviderError, build_forecast
from .schemas import (
    AlarmResponse,
    EdgeAnalyzeRequest,
    EdgeAnalyzeResponse,
    EdgeForecastRequest,
    EdgeForecastResponse,
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
    MediaAuthRequest,
    RefreshRequest,
    SessionResponse,
    SystemHealthResponse,
    WeatherObservationResponse,
    UserCreateRequest,
    UserResponse,
    UserUpdateRequest,
    VideoCredentialsResponse,
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
edge_analysis_lock = threading.Lock()
edge_analysis_times: dict[str, float] = {}


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
VIDEO_VIEWER_PATH = Path("/app/viewer/index.html")


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
@app.post("/api/v1/edge/ai/analyze", response_model=EdgeAnalyzeResponse)
def edge_ai_analyze(
    payload: EdgeAnalyzeRequest,
    request: Request,
    db: Session = Depends(get_db),
) -> EdgeAnalyzeResponse:
    """Accept a signed ESP snapshot and proxy it to the configured cloud model."""
    edge_key = request.headers.get("X-QXZ-Edge-Key", "")
    if not settings.edge_device_key:
        raise HTTPException(status_code=503, detail="Edge AI is not configured")
    if not hmac.compare_digest(edge_key, settings.edge_device_key):
        raise HTTPException(status_code=401, detail="Invalid edge key")

    request_data = payload.model_dump(mode="json")
    analysis = AIAnalysis(
        device_id=payload.device_id,
        snapshot_id=payload.snapshot_id,
        provider="openai-compatible",
        model=settings.llm_model,
        status="pending",
        request_payload=request_data,
    )
    db.add(analysis)

    if not payload.fusion.data_valid or payload.fusion.risk_level == "unknown":
        response = EdgeAnalyzeResponse(
            analysis_id=0,
            risk_level="unknown",
            summary="传感器数据不完整，暂不提供控制建议。",
            reasons=["风场或降雨传感器离线"],
            recommended_action="none",
            requires_confirmation=True,
        )
        analysis.status = "blocked"
        analysis.response_payload = response.model_dump(mode="json")
        db.commit()
        db.refresh(analysis)
        response.analysis_id = analysis.id
        analysis.response_payload = response.model_dump(mode="json")
        db.commit()
        return response

    now = time.monotonic()
    with edge_analysis_lock:
        last_request = edge_analysis_times.get(payload.device_id, 0.0)
        if now - last_request < settings.llm_device_cooldown_seconds:
            analysis.status = "cooldown"
            analysis.error_detail = "Device analysis cooldown is active"
            db.commit()
            raise HTTPException(status_code=429, detail="AI analysis is cooling down")
        edge_analysis_times[payload.device_id] = now

    try:
        provider_result = analyze_with_provider(payload)
    except AIProviderError as exc:
        analysis.status = "error"
        analysis.error_detail = str(exc)
        db.commit()
        logger.warning("AI analysis failed for %s: %s", payload.device_id, exc)
        raise HTTPException(status_code=503, detail="Cloud model analysis is unavailable") from exc

    result = provider_result.result.model_copy(update={"requires_confirmation": True})
    # Forecast advice is conservative: it can never open a landing pad and
    # dangerous short-term weather may only ask the operator to close, stop, or query.
    if payload.forecast is not None and payload.forecast.available:
        if result.recommended_action == "pad_open":
            result = result.model_copy(update={"recommended_action": "query_status"})
        if (payload.forecast.next_three_hour_risk == "danger" and
                result.recommended_action not in {"pad_close", "pad_stop", "query_status", "none"}):
            result = result.model_copy(update={"recommended_action": "query_status"})
    analysis.status = "success"
    analysis.duration_ms = provider_result.duration_ms
    analysis.response_payload = result.model_dump(mode="json")
    db.commit()
    db.refresh(analysis)
    return EdgeAnalyzeResponse(analysis_id=analysis.id, **result.model_dump())


@app.post("/api/v1/edge/weather/forecast", response_model=EdgeForecastResponse)
def edge_weather_forecast(
    payload: EdgeForecastRequest,
    request: Request,
    db: Session = Depends(get_db),
) -> EdgeForecastResponse:
    """Return cached Open-Meteo hourly data with conservative local correction."""
    forecast_key = request.headers.get("X-QXZ-Forecast-Key", "")
    if not settings.forecast_edge_key:
        raise HTTPException(status_code=503, detail="Forecast service is not configured")
    if not hmac.compare_digest(forecast_key, settings.forecast_edge_key):
        raise HTTPException(status_code=401, detail="Invalid forecast key")

    request_data = payload.model_dump(mode="json")
    snapshot = ForecastSnapshot(
        device_id=payload.device_id,
        latitude=payload.latitude,
        longitude=payload.longitude,
        timezone=payload.timezone,
        request_payload=request_data,
        response_payload={},
        status="pending",
    )
    db.add(snapshot)
    try:
        result = build_forecast(payload)
    except ForecastProviderError as exc:
        snapshot.status = "error"
        snapshot.error_detail = str(exc)
        db.commit()
        logger.warning("Forecast request failed for %s: %s", payload.device_id, exc)
        raise HTTPException(status_code=503, detail="Forecast provider is unavailable") from exc

    snapshot.status = "success"
    snapshot.response_payload = result.model_dump(mode="json")
    snapshot.expires_at = result.expires_at
    db.commit()
    return result


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


@app.get("/video/viewer.html", include_in_schema=False)
def video_viewer() -> FileResponse:
    if not VIDEO_VIEWER_PATH.is_file():
        raise HTTPException(status_code=503, detail="Video viewer is unavailable")
    return FileResponse(VIDEO_VIEWER_PATH, media_type="text/html", headers={"Cache-Control": "no-store"})


def forward_whep_request(
    method: str,
    upstream_path: str,
    body: bytes,
    request_headers: dict[str, str],
) -> tuple[int, bytes, dict[str, str]]:
    connection = http.client.HTTPConnection("qixiangzhan-video-mediamtx", 8889, timeout=15)
    try:
        connection.request(method, upstream_path, body=body, headers=request_headers)
        upstream = connection.getresponse()
        response_headers: dict[str, str] = {}
        for name in ("Content-Type", "Location"):
            value = upstream.getheader(name)
            if value:
                response_headers[name] = value
        return upstream.status, upstream.read(), response_headers
    finally:
        connection.close()


@app.api_route("/video/drone-01/whep{session_path:path}", methods=["POST", "DELETE"], include_in_schema=False)
async def video_whep_proxy(session_path: str, request: Request) -> Response:
    """Forward WHEP signaling while WebRTC media stays on MediaMTX/TURN."""
    upstream_path = f"/drone-01/whep{session_path}"
    request_headers = {"Content-Type": request.headers.get("content-type", "application/sdp")}
    if authorization := request.headers.get("authorization"):
        request_headers["Authorization"] = authorization

    try:
        status, body, response_headers = await asyncio.to_thread(
            forward_whep_request,
            request.method,
            upstream_path,
            await request.body(),
            request_headers,
        )
        if location := response_headers.get("Location"):
            parts = urlsplit(location)
            response_headers["Location"] = urlunsplit(("", "", f"/video{parts.path}", parts.query, ""))
        return Response(content=body, status_code=status, headers=response_headers)
    except OSError as exc:
        raise HTTPException(status_code=503, detail="Video signaling is unavailable") from exc


@app.post("/api/v1/devices/{device_id}/video/credentials", response_model=VideoCredentialsResponse)
def create_video_credentials(
    device_id: str,
    request: Request,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> VideoCredentialsResponse:
    ensure_device_access(db, current_user, device_id)
    stream_path = settings.video_stream_path.strip()
    if not stream_path:
        raise HTTPException(status_code=503, detail="Video stream path is not configured")
    username, password, expires_at = issue_media_credentials(current_user, device_id, stream_path)
    turn_username, turn_password = issue_turn_credentials(current_user)
    remote_addr, user_agent = request_meta(request)
    write_audit_log(
        db,
        user=current_user,
        event_type="video.credentials.issue",
        message="Issued short-lived video playback credentials",
        device_id=device_id,
        payload={"expires_at": expires_at.isoformat()},
        remote_addr=remote_addr,
        user_agent=user_agent,
    )
    db.commit()
    return VideoCredentialsResponse(
        device_id=device_id,
        whep_url=f"{settings.video_public_base.rstrip('/')}/{stream_path}/whep",
        username=username,
        password=password,
        expires_at=expires_at,
        turn_url=settings.turn_url,
        turn_username=turn_username,
        turn_password=turn_password,
    )


@app.post("/internal/media-auth")
def media_auth(payload: MediaAuthRequest, db: Session = Depends(get_db)) -> dict[str, bool]:
    # MediaMTX calls this endpoint from the private Docker network for every reader.
    viewer_user_id = media_ticket_user_id(payload.user, payload.path)
    viewer_device_id = media_ticket_device_id(payload.user, payload.path)
    viewer_user = db.get(User, viewer_user_id) if viewer_user_id is not None else None
    allowed = (
        payload.action == "read"
        and bool(payload.path)
        and verify_media_credentials(payload.user, payload.password, payload.path)
        and viewer_user is not None
        and viewer_user.is_active
        and viewer_device_id is not None
        and user_can_access_device(db, viewer_user, viewer_device_id)
    ) or (
        payload.action == "publish"
        and payload.path == "drone-01"
        and payload.user == settings.media_publish_username
        and payload.password == settings.media_publish_password
    )
    if not allowed:
        raise HTTPException(status_code=403, detail="Media authorization denied")
    return {"ok": True}


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
