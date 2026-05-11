import json
import secrets
from datetime import datetime, timedelta, timezone
from typing import Any, Callable

from sqlalchemy import Select, delete, func, select
from sqlalchemy.orm import Session

from .config import settings
from .models import (
    Alarm,
    AuditLog,
    CommandMessage,
    Device,
    DeviceLastState,
    DeviceMembership,
    RefreshToken,
    TelemetryMessage,
    SystemEvent,
    WeatherObservation,
    User,
)
from .project_storage import ProjectSessionLocal, ProjectTelemetry, project_database_label
from .realtime import realtime_hub

SUPPORTED_COMMANDS = {
    "set_r1",
    "set_r2",
    "set_all",
    "query_status",
    "pad_open",
    "pad_close",
    "pad_stop",
    "query_pad_status",
}

DEFAULT_WEATHER_CAPABILITIES = {
    "wind": True,
    "air": True,
    "rain": True,
    "pressure": False,
    "visibility": False,
}

DEFAULT_SENSOR_STATUS = {
    "wind_online": False,
    "air_online": False,
    "rain_online": False,
    "failure_count": 0,
    "last_ok_tick": 0,
    "last_error": "",
    "wind_last_tx_hex": "",
    "wind_last_rx_hex": "",
    "air_last_frame_hex": "",
    "l610_state": "",
}

PROJECT_STORAGE_FAILURE_ACTIVE = False


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


def naive_utc(value: datetime) -> datetime:
    return value.astimezone(timezone.utc).replace(tzinfo=None) if value.tzinfo is not None else value


def parse_json_payload(raw_payload: bytes) -> dict[str, Any] | str:
    text = raw_payload.decode("utf-8", errors="ignore").strip()
    try:
        payload = json.loads(text)
        if isinstance(payload, str):
            return json.loads(payload)
        return payload
    except json.JSONDecodeError:
        normalized = text
        if normalized.startswith('"') and normalized.endswith('"') and len(normalized) >= 2:
            normalized = normalized[1:-1]
        normalized = normalized.replace('\\"', '"').replace("\\\\", "\\")
        try:
            payload = json.loads(normalized)
            if isinstance(payload, str):
                return json.loads(payload)
            return payload
        except json.JSONDecodeError:
            return text


def mqtt_topic_kind(topic: str) -> str:
    if topic.endswith("/up/status"):
        return "status"
    if topic.startswith("device/") and topic.endswith("/status"):
        return "status"
    if topic.endswith("/up/ack"):
        return "ack"
    if topic.endswith("/up/online"):
        return "online"
    if topic.endswith("/up/event"):
        return "event"
    if topic.endswith("/down/cmd"):
        return "command"
    return "unknown"


def extract_device_id(topic: str) -> str:
    parts = topic.split("/")
    if len(parts) >= 4 and parts[0] == "device":
        return parts[1]
    return ""


def payload_dict(payload: dict[str, Any] | str) -> dict[str, Any]:
    return payload if isinstance(payload, dict) else {}


def normalize_payload_map(payload_map: dict[str, Any], topic: str) -> dict[str, Any]:
    normalized = dict(payload_map)

    if "device_id" not in normalized and payload_map.get("client_id"):
        normalized["device_id"] = payload_map.get("client_id")

    if mqtt_topic_kind(topic) == "status":
        normalized.setdefault("online", 1)
        net = payload_map.get("net")
        if isinstance(net, dict):
            if "rssi" not in normalized and "rssi" in net:
                normalized["rssi"] = net.get("rssi")
            if "operator" not in normalized and "operator" in net:
                normalized["operator"] = net.get("operator")
            if "ip" not in normalized and "ip" in net:
                normalized["ip"] = net.get("ip")

        pad = payload_map.get("pad")
        if isinstance(pad, dict):
            normalized["pad"] = {
                "left_state": str(pad.get("left_state") or pad.get("left") or "closed"),
                "right_state": str(pad.get("right_state") or pad.get("right") or "closed"),
                "ready": coerce_bool(pad.get("ready"), False),
                "occupied": coerce_bool(pad.get("occupied"), False),
                "mode": str(pad.get("mode") or "auto"),
            }

        weather = payload_map.get("weather")
        if isinstance(weather, dict):
            capabilities = weather_capabilities_from_payload(weather.get("capabilities"))
            sensor_status = sensor_status_from_payload(weather.get("sensor_status"))
            if not isinstance(weather.get("sensor_status"), dict):
                sensor_status["wind_online"] = capabilities["wind"]
                sensor_status["air_online"] = capabilities["air"]
                sensor_status["rain_online"] = capabilities["rain"]
            normalized["weather"] = {
                "wind_speed": coerce_float(weather.get("wind_speed"), 0.0),
                "wind_direction": coerce_float(weather.get("wind_direction"), 0.0),
                "wind_speed_raw": coerce_int(weather.get("wind_speed_raw"), 0),
                "wind_direction_raw": coerce_int(weather.get("wind_direction_raw"), 0),
                "rain_adc_raw": coerce_int(weather.get("rain_adc_raw"), 0),
                "wind_direction_text": str(weather.get("wind_direction_text") or ""),
                "rain_level_text": str(weather.get("rain_level_text") or ""),
                "temperature": coerce_float(weather.get("temperature"), 0.0),
                "humidity": coerce_float(weather.get("humidity"), 0.0),
                "pressure": coerce_float(weather.get("pressure"), 0.0),
                "visibility": coerce_float(weather.get("visibility"), 0.0),
                "capabilities": capabilities,
                "rain_detected": coerce_bool(weather.get("rain_detected"), False),
                "rain_value": coerce_float(weather.get("rain_value"), 0.0),
                "pm25": coerce_float(weather.get("pm25"), 0.0),
                "pm10": coerce_float(weather.get("pm10"), 0.0),
                "co2": coerce_float(weather.get("co2"), 0.0),
                "tvoc": coerce_float(weather.get("tvoc"), 0.0),
                "ch2o": coerce_float(weather.get("ch2o"), 0.0),
                "sensor_status": sensor_status,
            }

    return normalized


def is_legacy_status_topic(topic: str) -> bool:
    return topic.startswith("device/") and topic.endswith("/status") and not topic.endswith("/up/status")


def legacy_command_topic_for_status_topic(topic: str) -> str:
    if not is_legacy_status_topic(topic):
        return ""
    return topic[: -len("/status")] + "/cmd"


def legacy_command_token(command: str, value: Any) -> str:
    is_on = coerce_bool(value, False)
    if command == "set_r1":
        return "R1_ON" if is_on else "R1_OFF"
    if command == "set_r2":
        return "R2_ON" if is_on else "R2_OFF"
    if command == "set_all":
        return "ALL_ON" if is_on else "ALL_OFF"
    if command == "query_status":
        return "STATUS"
    return ""


def coerce_bool(value: Any, default: bool = False) -> bool:
    if value is None:
        return default
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return bool(value)
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in {"1", "true", "yes", "on"}:
            return True
        if normalized in {"0", "false", "no", "off"}:
            return False
    return default


def coerce_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def coerce_float(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def first_present(*values: Any) -> Any:
    for value in values:
        if value is not None and value != "":
            return value
    return None


def payload_child(payload_map: dict[str, Any], key: str) -> dict[str, Any]:
    value = payload_map.get(key)
    return value if isinstance(value, dict) else {}


def normalize_project_text(value: Any, default: str = "") -> str:
    if value is None:
        return default
    if isinstance(value, bool):
        return "on" if value else "off"
    return str(value)


def normalize_project_pressure(value: Any) -> str:
    if isinstance(value, bool):
        return "on" if value else "off"
    if isinstance(value, (int, float)):
        return "on" if int(value) != 0 else "off"
    text = str(value or "").strip()
    return text if text else "off"


def direction_to_degrees(value: Any) -> float:
    if value is None:
        return 0.0
    try:
        return float(value)
    except (TypeError, ValueError):
        pass

    text = str(value).strip().lower()
    mapping = {
        "north": 0.0,
        "n": 0.0,
        "east": 90.0,
        "e": 90.0,
        "south": 180.0,
        "s": 180.0,
        "west": 270.0,
        "w": 270.0,
        "northeast": 45.0,
        "ne": 45.0,
        "southeast": 135.0,
        "se": 135.0,
        "southwest": 225.0,
        "sw": 225.0,
        "northwest": 315.0,
        "nw": 315.0,
        "北": 0.0,
        "东": 90.0,
        "南": 180.0,
        "西": 270.0,
        "东北": 45.0,
        "东南": 135.0,
        "西南": 225.0,
        "西北": 315.0,
    }
    return mapping.get(text, 0.0)


def raindrop_detected(value: Any) -> bool:
    if value is None:
        return False
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    text = str(value).strip().lower()
    return text not in {"", "0", "false", "off", "none", "normal", "no", "无", "无雨"}


def extract_project_summary(payload_map: dict[str, Any]) -> dict[str, Any]:
    project = payload_child(payload_map, "project")
    weather = payload_child(payload_map, "weather")

    direction_value = first_present(
        project.get("direction"),
        payload_map.get("direction"),
        weather.get("wind_direction_text"),
        payload_map.get("weather_wind_direction_text"),
        weather.get("wind_direction"),
        payload_map.get("weather_wind_direction"),
    )
    raindrop_value = first_present(
        project.get("raindrop"),
        payload_map.get("raindrop"),
        weather.get("rain_level_text"),
        payload_map.get("weather_rain_level_text"),
        "Detected" if coerce_bool(weather.get("rain_detected", payload_map.get("weather_rain_detected")), False) else "",
    )

    return {
        "device_id": str(first_present(project.get("device_id"), payload_map.get("device_id")) or ""),
        "temperature": coerce_int(first_present(project.get("temperature"), payload_map.get("temperature"), weather.get("temperature"), payload_map.get("weather_temperature")), 0),
        "humidity": coerce_int(first_present(project.get("humidity"), payload_map.get("humidity"), weather.get("humidity"), payload_map.get("weather_humidity")), 0),
        "speed": coerce_int(first_present(project.get("speed"), payload_map.get("speed"), weather.get("wind_speed"), payload_map.get("weather_wind_speed")), 0),
        "direction": normalize_project_text(direction_value),
        "uv": normalize_project_text(first_present(project.get("uv"), payload_map.get("uv"))),
        "raindrop": normalize_project_text(raindrop_value),
        "pm": coerce_int(first_present(project.get("pm"), payload_map.get("pm"), weather.get("pm25"), payload_map.get("weather_pm25")), 0),
        "air_pressure": coerce_int(first_present(project.get("air_pressure"), payload_map.get("air_pressure"), weather.get("pressure"), payload_map.get("weather_pressure")), 0),
        "altitude": coerce_int(first_present(project.get("altitude"), payload_map.get("altitude")), 0),
        "pressure": normalize_project_pressure(first_present(project.get("pressure"), payload_map.get("pressure"), payload_map.get("pressure_status"), payload_map.get("weather_pressure_status"))),
        "distance": coerce_int(first_present(project.get("distance"), payload_map.get("distance"), weather.get("visibility"), payload_map.get("weather_visibility")), 0),
        "electric": coerce_int(first_present(project.get("electric"), payload_map.get("electric"), payload_map.get("battery"), payload_map.get("battery_percent")), 0),
        "posture": normalize_project_text(first_present(project.get("posture"), payload_map.get("posture"))),
        "complex": normalize_project_text(first_present(project.get("complex"), payload_map.get("complex"))),
        "longitude": coerce_float(first_present(project.get("longitude"), payload_map.get("longitude")), 0.0),
    }


def weather_capabilities_from_payload(value: Any) -> dict[str, bool]:
    capabilities = dict(DEFAULT_WEATHER_CAPABILITIES)

    if isinstance(value, dict):
        for key, fallback in DEFAULT_WEATHER_CAPABILITIES.items():
            capabilities[key] = coerce_bool(value.get(key), fallback)

    return capabilities


def sensor_status_from_payload(value: Any) -> dict[str, Any]:
    status: dict[str, Any] = dict(DEFAULT_SENSOR_STATUS)

    if not isinstance(value, dict):
        return status

    status["wind_online"] = coerce_bool(value.get("wind_online"), False)
    status["air_online"] = coerce_bool(value.get("air_online"), False)
    status["rain_online"] = coerce_bool(value.get("rain_online"), False)
    status["failure_count"] = coerce_int(value.get("failure_count"), 0)
    status["last_ok_tick"] = coerce_int(value.get("last_ok_tick"), 0)
    status["last_error"] = str(value.get("last_error") or "")
    status["wind_last_tx_hex"] = str(value.get("wind_last_tx_hex") or "")
    status["wind_last_rx_hex"] = str(value.get("wind_last_rx_hex") or "")
    status["air_last_frame_hex"] = str(value.get("air_last_frame_hex") or "")
    status["l610_state"] = str(value.get("l610_state") or "")
    return status


def normalize_pad_state(value: Any, fallback: str = "closed") -> str:
    text = str(value or fallback).strip().lower()
    allowed = {"closed", "opening", "open", "closing", "stopped", "fault"}
    return text if text in allowed else fallback


def extract_pad_summary(payload_map: dict[str, Any]) -> dict[str, Any]:
    pad = payload_map.get("pad")
    if not isinstance(pad, dict):
        pad = {}

    return {
        "left_state": normalize_pad_state(pad.get("left_state") or payload_map.get("pad_left_state"), "closed"),
        "right_state": normalize_pad_state(pad.get("right_state") or payload_map.get("pad_right_state"), "closed"),
        "ready": coerce_bool(pad.get("ready", payload_map.get("pad_ready")), False),
        "occupied": coerce_bool(pad.get("occupied", payload_map.get("pad_occupied")), False),
        "mode": str(pad.get("mode") or payload_map.get("pad_mode") or "auto"),
    }


def extract_weather_summary(payload_map: dict[str, Any]) -> dict[str, Any]:
    weather = payload_map.get("weather")
    if not isinstance(weather, dict):
        weather = {}
    project = extract_project_summary(payload_map)
    capabilities = weather_capabilities_from_payload(
        weather.get("capabilities")
        or {
            "wind": payload_map.get("weather_capability_wind"),
            "air": payload_map.get("weather_capability_air"),
            "rain": payload_map.get("weather_capability_rain"),
            "pressure": payload_map.get("weather_capability_pressure"),
            "visibility": payload_map.get("weather_capability_visibility"),
        }
    )
    project_payload = payload_child(payload_map, "project")
    if "air_pressure" in project_payload or "air_pressure" in payload_map:
        capabilities["pressure"] = True
    if "distance" in project_payload or "distance" in payload_map:
        capabilities["visibility"] = True
    sensor_status = sensor_status_from_payload(
        weather.get("sensor_status", payload_map.get("weather_sensor_status"))
    )
    if not isinstance(weather.get("sensor_status", payload_map.get("weather_sensor_status")), dict):
        sensor_status["wind_online"] = capabilities["wind"]
        sensor_status["air_online"] = capabilities["air"]
        sensor_status["rain_online"] = capabilities["rain"]

    return {
        "wind_speed": coerce_float(first_present(weather.get("wind_speed"), payload_map.get("weather_wind_speed"), project["speed"]), 0.0),
        "wind_direction": coerce_float(first_present(weather.get("wind_direction"), payload_map.get("weather_wind_direction"), direction_to_degrees(project["direction"])), 0.0),
        "wind_speed_raw": coerce_int(weather.get("wind_speed_raw", payload_map.get("weather_wind_speed_raw")), 0),
        "wind_direction_raw": coerce_int(weather.get("wind_direction_raw", payload_map.get("weather_wind_direction_raw")), 0),
        "rain_adc_raw": coerce_int(weather.get("rain_adc_raw", payload_map.get("weather_rain_adc_raw")), 0),
        "wind_direction_text": str(first_present(weather.get("wind_direction_text"), payload_map.get("weather_wind_direction_text"), project["direction"]) or ""),
        "rain_level_text": str(first_present(weather.get("rain_level_text"), payload_map.get("weather_rain_level_text"), project["raindrop"]) or ""),
        "temperature": coerce_float(first_present(weather.get("temperature"), payload_map.get("weather_temperature"), project["temperature"]), 0.0),
        "humidity": coerce_float(first_present(weather.get("humidity"), payload_map.get("weather_humidity"), project["humidity"]), 0.0),
        "pressure": coerce_float(first_present(weather.get("pressure"), payload_map.get("weather_pressure"), project["air_pressure"]), 0.0),
        "visibility": coerce_float(first_present(weather.get("visibility"), payload_map.get("weather_visibility"), project["distance"]), 0.0),
        "capabilities": capabilities,
        "rain_detected": coerce_bool(first_present(weather.get("rain_detected"), payload_map.get("weather_rain_detected"), raindrop_detected(project["raindrop"])), False),
        "rain_value": coerce_float(weather.get("rain_value", payload_map.get("weather_rain_value")), 0.0),
        "pm25": coerce_float(first_present(weather.get("pm25"), payload_map.get("weather_pm25"), project["pm"]), 0.0),
        "pm10": coerce_float(weather.get("pm10", payload_map.get("weather_pm10")), 0.0),
        "co2": coerce_float(weather.get("co2", payload_map.get("weather_co2")), 0.0),
        "tvoc": coerce_float(weather.get("tvoc", payload_map.get("weather_tvoc")), 0.0),
        "ch2o": coerce_float(weather.get("ch2o", payload_map.get("weather_ch2o")), 0.0),
        "sensor_status": sensor_status,
    }


def build_command_msg_id(device_id: str) -> str:
    return f"{device_id}-{int(utcnow().timestamp() * 1000)}-{secrets.token_hex(3)}"


def default_command_topic(device_id: str) -> str:
    return f"device/{device_id}/down/cmd"


def preferred_command_topic(db: Session, device_id: str) -> str:
    latest_status_topic = db.scalar(
        select(TelemetryMessage.topic)
        .where(
            TelemetryMessage.device_id == device_id,
            TelemetryMessage.event_type == "status",
            TelemetryMessage.direction == "up",
        )
        .order_by(TelemetryMessage.created_at.desc())
        .limit(1)
    )
    if latest_status_topic and is_legacy_status_topic(latest_status_topic):
        legacy_topic = legacy_command_topic_for_status_topic(latest_status_topic)
        if legacy_topic:
            return legacy_topic
    return default_command_topic(device_id)


def normalize_command_value(command: str, value: Any) -> str:
    if command in {"query_status", "pad_open", "pad_close", "pad_stop", "query_pad_status"}:
        return ""
    if isinstance(value, bool):
        return "1" if value else "0"
    return str(value if value is not None else "")


def is_pending_command_status(status: str) -> bool:
    return status in {"pending", "queued", "sent"}


def command_matches_status(command_row: CommandMessage, payload_map: dict[str, Any]) -> bool:
    if command_row.command == "query_status":
        return True
    if command_row.command == "query_pad_status":
        return True

    pad = extract_pad_summary(payload_map)
    left_state = pad["left_state"]
    right_state = pad["right_state"]

    if command_row.command == "pad_open":
        return left_state == "open" and right_state == "open"
    if command_row.command == "pad_close":
        return left_state == "closed" and right_state == "closed"
    if command_row.command == "pad_stop":
        stable_states = {"open", "closed", "stopped", "fault"}
        if left_state == "stopped" or right_state == "stopped":
            return True
        return left_state in stable_states and right_state in stable_states

    desired = coerce_bool(command_row.value, False)
    relay1 = coerce_bool(payload_map.get("relay1"), False)
    relay2 = coerce_bool(payload_map.get("relay2"), False)

    if command_row.command == "set_r1":
        return relay1 == desired
    if command_row.command == "set_r2":
        return relay2 == desired
    if command_row.command == "set_all":
        return relay1 == desired and relay2 == desired
    return False


def status_snapshot_is_fresh(device: Device, payload_map: dict[str, Any]) -> bool:
    incoming_tick = coerce_int(payload_map.get("tick"), device.tick)
    if incoming_tick != device.tick:
        return True

    incoming_online = coerce_bool(payload_map.get("online"), device.online)
    if incoming_online != device.online:
        return True

    incoming_rssi = coerce_int(payload_map.get("rssi"), device.rssi)
    if incoming_rssi != device.rssi:
        return True

    if str(payload_map.get("ip") or device.ip) != device.ip:
        return True
    if str(payload_map.get("state") or device.state_text) != device.state_text:
        return True

    relay1 = coerce_bool(payload_map.get("relay1"), device.relay1)
    relay2 = coerce_bool(payload_map.get("relay2"), device.relay2)
    if relay1 != device.relay1 or relay2 != device.relay2:
        return True

    pad = extract_pad_summary(payload_map)
    if (
        pad["left_state"] != device.pad_left_state
        or pad["right_state"] != device.pad_right_state
        or pad["ready"] != device.pad_ready
        or pad["occupied"] != device.pad_occupied
        or pad["mode"] != device.pad_mode
    ):
        return True

    return False


def resolve_pending_commands_from_status(
    db: Session,
    *,
    device_id: str,
    payload_map: dict[str, Any],
    now: datetime,
    fresh_status: bool,
) -> list[CommandMessage]:
    pending_rows = db.scalars(
        select(CommandMessage)
        .where(
            CommandMessage.device_id == device_id,
            CommandMessage.direction == "down",
            CommandMessage.acked_at.is_(None),
            CommandMessage.status.in_(("pending", "queued", "sent")),
        )
        .order_by(CommandMessage.created_at.asc())
    ).all()

    resolved: list[CommandMessage] = []
    for row in pending_rows:
        if row.command in {"query_status", "query_pad_status"}:
            if not fresh_status:
                continue
        elif not fresh_status:
            continue

        if not command_matches_status(row, payload_map):
            continue
        row.status = "success"
        row.detail = "Resolved from status report"
        row.acked_at = now
        resolved.append(row)

    if resolved:
        set_alarm_state(db, device_id, "ack_timeout", "critical", "ACK recovered", "backend", False)
        set_alarm_state(db, device_id, "command_error", "critical", "Command execution recovered", "backend", False)

    return resolved


def validate_command_request(command: str, value: Any) -> None:
    if command not in SUPPORTED_COMMANDS:
        raise ValueError(f"Unsupported command: {command}")
    if command not in {"query_status", "pad_open", "pad_close", "pad_stop", "query_pad_status"} and value is None:
        raise ValueError(f"Command {command} requires a value")


def create_command_payload(
    *,
    topic: str,
    msg_id: str,
    device_id: str,
    command: str,
    value: Any,
    operator: str,
    now: datetime,
) -> tuple[dict[str, Any] | str, str]:
    if topic.endswith("/cmd") and not topic.endswith("/down/cmd"):
        token = legacy_command_token(command, value)
        if not token:
            raise ValueError(f"Legacy topic does not support command: {command}")
        return token, token

    payload: dict[str, Any] = {
        "msg_id": msg_id,
        "device_id": device_id,
        "cmd": command,
        "operator": operator,
        "timestamp": now.isoformat(),
    }
    if command not in {"query_status", "pad_open", "pad_close", "pad_stop", "query_pad_status"}:
        payload["value"] = coerce_int(value, 0) if isinstance(value, (bool, int, str)) else value
    encoded = json.dumps(payload, ensure_ascii=False, separators=(",", ":"))
    return payload, encoded


def serialize_project_telemetry(project: ProjectTelemetry | None) -> dict[str, Any] | None:
    if project is None:
        return None
    return {
        "id": int(project.id),
        "device_id": project.device_id,
        "temperature": int(project.temperature),
        "humidity": int(project.humidity),
        "speed": int(project.speed),
        "direction": project.direction,
        "uv": project.uv,
        "raindrop": project.raindrop,
        "pm": int(project.pm),
        "air_pressure": int(project.air_pressure),
        "altitude": int(project.altitude),
        "pressure": project.pressure,
        "distance": int(project.distance),
        "electric": int(project.electric),
        "posture": project.posture,
        "complex": project.complex,
        "longitude": float(project.longitude),
    }


def serialize_device(device: Device, active_alarm_count: int = 0, project: ProjectTelemetry | None = None) -> dict[str, Any]:
    sensor_status = sensor_status_from_payload(device.weather_sensor_status or {})
    if not device.weather_sensor_status:
        sensor_status["wind_online"] = device.weather_capability_wind
        sensor_status["air_online"] = device.weather_capability_air
        sensor_status["rain_online"] = device.weather_capability_rain

    return {
        "device_id": device.device_id,
        "display_name": device.display_name,
        "operator_name": device.operator_name,
        "ip": device.ip,
        "online": device.online,
        "rssi": device.rssi,
        "relay1": device.relay1,
        "relay2": device.relay2,
        "pad": {
            "left_state": device.pad_left_state,
            "right_state": device.pad_right_state,
            "ready": device.pad_ready,
            "occupied": device.pad_occupied,
            "mode": device.pad_mode,
        },
        "weather": {
            "wind_speed": device.weather_wind_speed,
            "wind_direction": device.weather_wind_direction,
            "wind_speed_raw": int(device.weather_wind_speed_raw),
            "wind_direction_raw": int(device.weather_wind_direction_raw),
            "rain_adc_raw": int(device.weather_rain_adc_raw),
            "wind_direction_text": device.weather_wind_direction_text,
            "rain_level_text": device.weather_rain_level_text,
            "temperature": device.weather_temperature,
            "humidity": device.weather_humidity,
            "pressure": device.weather_pressure,
            "visibility": device.weather_visibility,
            "capabilities": {
                "wind": device.weather_capability_wind,
                "air": device.weather_capability_air,
                "rain": device.weather_capability_rain,
                "pressure": device.weather_capability_pressure,
                "visibility": device.weather_capability_visibility,
            },
            "rain_detected": device.weather_rain_detected,
            "rain_value": device.weather_rain_value,
            "pm25": device.weather_pm25,
            "pm10": device.weather_pm10,
            "co2": device.weather_co2,
            "tvoc": device.weather_tvoc,
            "ch2o": device.weather_ch2o,
            "sensor_status": sensor_status,
        },
        "project": serialize_project_telemetry(project),
        "state_text": device.state_text,
        "tick": device.tick,
        "protocol_profile": device.protocol_profile,
        "legacy_status_topic": device.legacy_status_topic,
        "legacy_command_topic": device.legacy_command_topic,
        "last_seen_at": device.last_seen_at,
        "updated_at": device.updated_at,
        "active_alarm_count": active_alarm_count,
    }


def serialize_command(command: CommandMessage) -> dict[str, Any]:
    return {
        "id": command.id,
        "msg_id": command.msg_id,
        "device_id": command.device_id,
        "command": command.command,
        "value": command.value,
        "direction": command.direction,
        "status": command.status,
        "operator_name": command.operator_name,
        "detail": command.detail,
        "topic": command.topic,
        "payload": command.payload,
        "created_at": command.created_at,
        "acked_at": command.acked_at,
    }


def serialize_user(user: User, device_ids: list[str]) -> dict[str, Any]:
    return {
        "id": user.id,
        "username": user.username,
        "display_name": user.display_name,
        "role": user.role,
        "is_active": user.is_active,
        "created_at": user.created_at,
        "last_login_at": user.last_login_at,
        "device_ids": sorted(device_ids),
    }


def allowed_device_ids_for_user(db: Session, user: User) -> set[str]:
    if user.role == "admin":
        return set(db.scalars(select(Device.device_id)).all())
    return set(
        db.scalars(select(DeviceMembership.device_id).where(DeviceMembership.user_id == user.id)).all()
    )


def user_can_access_device(db: Session, user: User, device_id: str) -> bool:
    if user.role == "admin":
        return True
    return db.scalar(
        select(func.count(DeviceMembership.id)).where(
            DeviceMembership.user_id == user.id,
            DeviceMembership.device_id == device_id,
        )
    ) > 0


def device_ids_for_user(db: Session, user_id: int) -> list[str]:
    return db.scalars(
        select(DeviceMembership.device_id).where(DeviceMembership.user_id == user_id).order_by(DeviceMembership.device_id)
    ).all()


def replace_user_memberships(db: Session, user_id: int, device_ids: list[str]) -> None:
    normalized = sorted({item.strip() for item in device_ids if item and item.strip()})
    existing = db.scalars(select(DeviceMembership).where(DeviceMembership.user_id == user_id)).all()
    existing_by_device = {row.device_id: row for row in existing}

    for device_id in list(existing_by_device):
        if device_id not in normalized:
            db.delete(existing_by_device[device_id])

    for device_id in normalized:
        if device_id in existing_by_device:
            continue
        db.add(DeviceMembership(user_id=user_id, device_id=device_id, created_at=utcnow()))


def write_audit_log(
    db: Session,
    *,
    user: User | None,
    event_type: str,
    message: str,
    severity: str = "info",
    device_id: str = "",
    payload: dict[str, Any] | str | None = None,
    remote_addr: str = "",
    user_agent: str = "",
) -> None:
    db.add(
        AuditLog(
            user_id=user.id if user is not None else None,
            username=user.username if user is not None else "",
            role=user.role if user is not None else "",
            device_id=device_id,
            event_type=event_type,
            severity=severity,
            remote_addr=remote_addr,
            user_agent=user_agent,
            message=message,
            payload=payload or {},
            created_at=utcnow(),
        )
    )


def emit_device_events(db: Session, device_id: str, command_row: CommandMessage | None = None) -> None:
    device = db.scalar(select(Device).where(Device.device_id == device_id))
    if device is None:
        return

    alarm_count = db.scalar(
        select(func.count(Alarm.id)).where(Alarm.device_id == device_id, Alarm.active.is_(True))
    ) or 0
    alarms = db.scalars(
        select(Alarm).where(Alarm.device_id == device_id, Alarm.active.is_(True)).order_by(Alarm.created_at.desc()).limit(10)
    ).all()
    realtime_hub.publish(
        "device_state",
        device_id=device_id,
        payload=serialize_device(device, int(alarm_count)),
    )
    realtime_hub.publish(
        "alarm_update",
        device_id=device_id,
        payload={
            "active_alarm_count": int(alarm_count),
            "items": [
                {
                    "alarm_key": alarm.alarm_key,
                    "code": alarm.code,
                    "severity": alarm.severity,
                    "message": alarm.message,
                    "active": alarm.active,
                    "created_at": alarm.created_at,
                    "resolved_at": alarm.resolved_at,
                }
                for alarm in alarms
            ],
        },
    )
    if command_row is not None:
        realtime_hub.publish("command_update", device_id=device_id, payload=serialize_command(command_row))


def get_or_create_device(db: Session, device_id: str) -> Device:
    device = db.scalar(select(Device).where(Device.device_id == device_id))
    if device is None:
        device = Device(device_id=device_id, display_name=device_id)
        db.add(device)
        db.flush()
    return device


def set_alarm_state(
    db: Session,
    device_id: str,
    code: str,
    severity: str,
    message: str,
    source: str,
    active: bool,
) -> None:
    alarm_key = f"{device_id}::{code}"
    alarm = db.scalar(select(Alarm).where(Alarm.alarm_key == alarm_key))

    if alarm is None:
        alarm = Alarm(
            alarm_key=alarm_key,
            device_id=device_id,
            code=code,
            severity=severity,
            message=message,
            source=source,
            active=active,
            created_at=utcnow(),
            resolved_at=None if active else utcnow(),
        )
        db.add(alarm)
        db.flush()
        return

    alarm.severity = severity
    alarm.message = message
    alarm.source = source
    alarm.active = active
    if active:
        alarm.resolved_at = None
    else:
        alarm.resolved_at = utcnow()


def update_last_state(db: Session, device_id: str, payload: dict[str, Any]) -> None:
    row = db.scalar(select(DeviceLastState).where(DeviceLastState.device_id == device_id))
    if row is None:
        row = DeviceLastState(device_id=device_id, payload=payload, updated_at=utcnow())
        db.add(row)
        return

    row.payload = payload
    row.updated_at = utcnow()


def payload_timestamp(payload_map: dict[str, Any], fallback: datetime) -> datetime:
    value = payload_map.get("timestamp")
    if not value:
        return fallback
    try:
        text = str(value).replace("Z", "+00:00")
        parsed = datetime.fromisoformat(text)
        return parsed if parsed.tzinfo is not None else parsed.replace(tzinfo=timezone.utc)
    except (TypeError, ValueError):
        return fallback


def upsert_weather_observation(
    db: Session,
    *,
    device_id: str,
    msg_id: str | None,
    topic: str,
    payload: dict[str, Any] | str,
    payload_map: dict[str, Any],
    weather: dict[str, Any],
    now: datetime,
) -> None:
    observation = None
    if msg_id:
        observation = db.scalar(
            select(WeatherObservation).where(
                WeatherObservation.device_id == device_id,
                WeatherObservation.msg_id == msg_id,
                WeatherObservation.topic == topic,
            )
        )

    if observation is None:
        observation = WeatherObservation(
            device_id=device_id,
            msg_id=msg_id,
            topic=topic,
            observed_at=payload_timestamp(payload_map, now),
            created_at=now,
            capabilities=weather["capabilities"],
            sensor_status=weather["sensor_status"],
            payload=payload,
        )
        db.add(observation)

    observation.wind_speed = weather["wind_speed"]
    observation.wind_direction = weather["wind_direction"]
    observation.wind_speed_raw = weather["wind_speed_raw"]
    observation.wind_direction_raw = weather["wind_direction_raw"]
    observation.rain_adc_raw = weather["rain_adc_raw"]
    observation.wind_direction_text = weather["wind_direction_text"]
    observation.rain_level_text = weather["rain_level_text"]
    observation.temperature = weather["temperature"]
    observation.humidity = weather["humidity"]
    observation.pressure = weather["pressure"]
    observation.visibility = weather["visibility"]
    observation.rain_detected = weather["rain_detected"]
    observation.rain_value = weather["rain_value"]
    observation.pm25 = weather["pm25"]
    observation.pm10 = weather["pm10"]
    observation.co2 = weather["co2"]
    observation.tvoc = weather["tvoc"]
    observation.ch2o = weather["ch2o"]
    observation.capabilities = weather["capabilities"]
    observation.sensor_status = weather["sensor_status"]
    observation.payload = payload


def write_project_telemetry(
    db: Session,
    *,
    device_id: str,
    payload_map: dict[str, Any],
) -> None:
    global PROJECT_STORAGE_FAILURE_ACTIVE

    project = extract_project_summary(payload_map)
    project["device_id"] = device_id

    try:
        with ProjectSessionLocal() as project_db:
            project_db.add(ProjectTelemetry(**project))
            project_db.commit()
    except Exception as exc:  # pragma: no cover - depends on optional external database
        if not PROJECT_STORAGE_FAILURE_ACTIVE:
            create_system_event(
                db,
                event_type="project.write_failed",
                severity="warning",
                source="project-db",
                message="weather.project write failed; main telemetry pipeline is still running",
                payload={
                    "database_url": project_database_label(),
                    "error": str(exc),
                },
            )
            PROJECT_STORAGE_FAILURE_ACTIVE = True
        return

    if PROJECT_STORAGE_FAILURE_ACTIVE:
        create_system_event(
            db,
            event_type="project.write_recovered",
            severity="info",
            source="project-db",
            message="weather.project write recovered",
            payload={"database_url": project_database_label()},
        )
    PROJECT_STORAGE_FAILURE_ACTIVE = False


def create_system_event(
    db: Session,
    *,
    event_type: str,
    message: str,
    severity: str = "info",
    source: str = "backend",
    payload: dict[str, Any] | str | None = None,
    commit: bool = False,
) -> None:
    db.add(
        SystemEvent(
            event_type=event_type,
            severity=severity,
            source=source,
            message=message,
            payload=payload or {},
            created_at=utcnow(),
        )
    )
    if commit:
        db.commit()


def prune_runtime_history(db: Session, *, retention_days: int | None = None) -> dict[str, int]:
    days = retention_days if retention_days is not None else settings.retention_days
    threshold = naive_utc(utcnow() - timedelta(days=days))
    deleted: dict[str, int] = {}
    for model, label in (
        (WeatherObservation, "weather_observations"),
        (TelemetryMessage, "telemetry_messages"),
        (CommandMessage, "command_messages"),
        (SystemEvent, "system_events"),
    ):
        result = db.execute(delete(model).where(model.created_at < threshold))
        deleted[label] = int(result.rowcount or 0)

    create_system_event(
        db,
        event_type="retention.cleanup",
        message=f"Pruned records older than {days} days",
        payload={"deleted": deleted, "retention_days": days},
    )
    db.commit()
    return deleted


def upsert_telemetry_message(
    db: Session,
    *,
    device_id: str,
    msg_id: str | None,
    topic: str,
    direction: str,
    topic_kind: str,
    payload: dict[str, Any] | str,
    payload_map: dict[str, Any],
    now: datetime,
) -> None:
    telemetry = None
    if msg_id:
        telemetry = db.scalar(
            select(TelemetryMessage).where(
                TelemetryMessage.device_id == device_id,
                TelemetryMessage.msg_id == msg_id,
                TelemetryMessage.topic == topic,
                TelemetryMessage.direction == direction,
            )
        )

    if telemetry is None:
        telemetry = TelemetryMessage(
            device_id=device_id,
            msg_id=msg_id,
            topic=topic,
            direction=direction,
            channel="mqtt",
            event_type=topic_kind,
            command=str(payload_map.get("cmd") or payload_map.get("event") or ""),
            result=str(payload_map.get("result") or ""),
            operator_name=str(payload_map.get("operator") or ""),
            payload=payload,
            created_at=now,
        )
        db.add(telemetry)
        return

    telemetry.event_type = topic_kind
    telemetry.command = str(payload_map.get("cmd") or payload_map.get("event") or "")
    telemetry.result = str(payload_map.get("result") or "")
    telemetry.operator_name = str(payload_map.get("operator") or "")
    telemetry.payload = payload


def record_message(db: Session, topic: str, raw_payload: bytes) -> None:
    payload = parse_json_payload(raw_payload)
    payload_map = normalize_payload_map(payload_dict(payload), topic)
    device_id = str(payload_map.get("device_id") or extract_device_id(topic))
    topic_kind = mqtt_topic_kind(topic)
    direction = "down" if "/down/" in topic else "up"
    now = utcnow()

    if not device_id:
        return

    device = get_or_create_device(db, device_id)
    device.updated_at = now
    if direction == "up":
        device.last_seen_at = now
    resolved_command_rows: list[CommandMessage] = []

    msg_id = str(payload_map.get("msg_id") or "") or None
    upsert_telemetry_message(
        db,
        device_id=device_id,
        msg_id=msg_id,
        topic=topic,
        direction=direction,
        topic_kind=topic_kind,
        payload=payload,
        payload_map=payload_map,
        now=now,
    )

    if topic_kind == "command":
        command = str(payload_map.get("cmd") or "")
        command_row = db.scalar(select(CommandMessage).where(CommandMessage.msg_id == msg_id)) if msg_id else None
        if command_row is None:
            command_row = CommandMessage(
                msg_id=msg_id,
                device_id=device_id,
                command=command,
                value=str(payload_map.get("value") or ""),
                direction="down",
                status="pending",
                operator_name=str(payload_map.get("operator") or ""),
                detail="",
                topic=topic,
                payload=payload,
                created_at=now,
            )
            db.add(command_row)
        else:
            command_row.topic = topic
            command_row.payload = payload
            command_row.command = command
            command_row.value = str(payload_map.get("value") or "")
            command_row.operator_name = str(payload_map.get("operator") or "")
            command_row.status = "pending"

    if topic_kind == "status":
        fresh_status = status_snapshot_is_fresh(device, payload_map)
        pad = extract_pad_summary(payload_map)
        weather = extract_weather_summary(payload_map)
        device.display_name = str(payload_map.get("display_name") or device.display_name or device_id)
        device.operator_name = str(payload_map.get("operator") or "")
        device.ip = str(payload_map.get("ip") or "")
        device.online = coerce_bool(payload_map.get("online"), True)
        device.rssi = coerce_int(payload_map.get("rssi"), 0)
        device.relay1 = coerce_bool(payload_map.get("relay1"), False)
        device.relay2 = coerce_bool(payload_map.get("relay2"), False)
        device.pad_left_state = pad["left_state"]
        device.pad_right_state = pad["right_state"]
        device.pad_ready = pad["ready"]
        device.pad_occupied = pad["occupied"]
        device.pad_mode = pad["mode"]
        device.weather_wind_speed = weather["wind_speed"]
        device.weather_wind_direction = weather["wind_direction"]
        device.weather_wind_speed_raw = weather["wind_speed_raw"]
        device.weather_wind_direction_raw = weather["wind_direction_raw"]
        device.weather_rain_adc_raw = weather["rain_adc_raw"]
        device.weather_wind_direction_text = weather["wind_direction_text"]
        device.weather_rain_level_text = weather["rain_level_text"]
        device.weather_temperature = weather["temperature"]
        device.weather_humidity = weather["humidity"]
        device.weather_pressure = weather["pressure"]
        device.weather_visibility = weather["visibility"]
        device.weather_capability_wind = weather["capabilities"]["wind"]
        device.weather_capability_air = weather["capabilities"]["air"]
        device.weather_capability_rain = weather["capabilities"]["rain"]
        device.weather_capability_pressure = weather["capabilities"]["pressure"]
        device.weather_capability_visibility = weather["capabilities"]["visibility"]
        device.weather_rain_detected = weather["rain_detected"]
        device.weather_rain_value = weather["rain_value"]
        device.weather_pm25 = weather["pm25"]
        device.weather_pm10 = weather["pm10"]
        device.weather_co2 = weather["co2"]
        device.weather_tvoc = weather["tvoc"]
        device.weather_ch2o = weather["ch2o"]
        device.weather_sensor_status = weather["sensor_status"]
        device.state_text = str(payload_map.get("state") or "")
        device.tick = coerce_int(payload_map.get("tick"), 0)
        if is_legacy_status_topic(topic):
            device.protocol_profile = "legacy"
            device.legacy_status_topic = topic
            device.legacy_command_topic = legacy_command_topic_for_status_topic(topic)
        elif "pad" in payload_map:
            device.protocol_profile = "airport_pad_v1"
        else:
            device.protocol_profile = "relay_v1"
        update_last_state(db, device_id, payload_map)
        upsert_weather_observation(
            db,
            device_id=device_id,
            msg_id=msg_id,
            topic=topic,
            payload=payload,
            payload_map=payload_map,
            weather=weather,
            now=now,
        )
        write_project_telemetry(
            db,
            device_id=device_id,
            payload_map=payload_map,
        )
        resolved_command_rows = resolve_pending_commands_from_status(
            db,
            device_id=device_id,
            payload_map=payload_map,
            now=now,
            fresh_status=fresh_status,
        )

        if 0 < device.rssi < 10:
            set_alarm_state(db, device_id, "low_rssi", "warning", f"Device RSSI is low: {device.rssi}", "mqtt", True)
        else:
            set_alarm_state(db, device_id, "low_rssi", "warning", "Device RSSI recovered", "mqtt", False)

        sensor_status = weather["sensor_status"]
        weather_stale = False
        for sensor_key, capability_key, label in (
            ("wind_online", "wind", "Wind sensor"),
            ("air_online", "air", "Air sensor"),
            ("rain_online", "rain", "Rain sensor"),
        ):
            alarm_code = sensor_key.replace("_online", "_sensor_offline")
            should_check = bool(weather["capabilities"].get(capability_key))
            sensor_online = coerce_bool(sensor_status.get(sensor_key), should_check)
            if should_check and not sensor_online:
                weather_stale = True
                set_alarm_state(db, device_id, alarm_code, "warning", f"{label} offline", "mqtt", True)
            else:
                set_alarm_state(db, device_id, alarm_code, "warning", f"{label} recovered", "mqtt", False)

        if device.online and weather_stale:
            set_alarm_state(db, device_id, "weather_stale", "warning", "Device online but weather data is degraded", "mqtt", True)
        else:
            set_alarm_state(db, device_id, "weather_stale", "warning", "Weather data recovered", "mqtt", False)

    if topic_kind == "ack":
        if msg_id:
            command_row = db.scalar(select(CommandMessage).where(CommandMessage.msg_id == msg_id))
            if command_row is None:
                command_row = CommandMessage(
                    msg_id=msg_id,
                    device_id=device_id,
                    command=str(payload_map.get("cmd") or ""),
                    value=str(payload_map.get("value") or ""),
                    direction="down",
                    status=str(payload_map.get("result") or "ack"),
                    operator_name=str(payload_map.get("operator") or ""),
                    detail=str(payload_map.get("detail") or ""),
                    topic=topic,
                    payload=payload,
                    created_at=now,
                    acked_at=now,
                )
                db.add(command_row)
            else:
                command_row.status = str(payload_map.get("result") or command_row.status)
                command_row.detail = str(payload_map.get("detail") or "")
                command_row.acked_at = now
                command_row.payload = payload
                command_row.topic = topic

        result = str(payload_map.get("result") or "")
        if result and result.lower() != "success":
            set_alarm_state(
                db,
                device_id,
                "command_error",
                "critical",
                f"{payload_map.get('cmd', 'command')} failed: {payload_map.get('detail', '')}",
                "mqtt",
                True,
            )
        else:
            set_alarm_state(db, device_id, "command_error", "critical", "Command execution recovered", "mqtt", False)
            set_alarm_state(db, device_id, "ack_timeout", "critical", "ACK recovered", "mqtt", False)

    if topic_kind == "online":
        device.online = coerce_bool(payload_map.get("online"), False)
        if device.online:
            set_alarm_state(db, device_id, "device_offline", "critical", "Device reconnected", "mqtt", False)
        else:
            set_alarm_state(db, device_id, "device_offline", "critical", "Device offline", "mqtt", True)

    db.commit()
    if topic_kind in {"status", "ack", "online", "command"}:
        if resolved_command_rows:
            for row in resolved_command_rows:
                emit_device_events(db, device_id, command_row=row)
        else:
            command_row = db.scalar(select(CommandMessage).where(CommandMessage.msg_id == msg_id)) if msg_id else None
            emit_device_events(db, device_id, command_row=command_row)


def create_command_request(
    db: Session,
    *,
    device_id: str,
    command: str,
    value: Any,
    operator: str,
    publish: Callable[[str, bytes | str], tuple[bool, str]],
) -> CommandMessage:
    validate_command_request(command, value)

    now = utcnow()
    device = get_or_create_device(db, device_id)
    msg_id = build_command_msg_id(device_id)
    topic = preferred_command_topic(db, device_id)
    payload, encoded_payload = create_command_payload(
        topic=topic,
        msg_id=msg_id,
        device_id=device_id,
        command=command,
        value=value,
        operator=operator,
        now=now,
    )

    command_row = CommandMessage(
        msg_id=msg_id,
        device_id=device_id,
        command=command,
        value=normalize_command_value(command, value),
        direction="down",
        status="pending",
        operator_name=operator,
        detail="",
        topic=topic,
        payload=payload,
        created_at=now,
    )
    db.add(command_row)

    if topic.endswith("/cmd") and not topic.endswith("/down/cmd"):
        device.protocol_profile = "legacy"
        device.legacy_command_topic = topic

    if topic.endswith("/cmd") and not topic.endswith("/down/cmd"):
        db.add(
            TelemetryMessage(
                device_id=device_id,
                msg_id=msg_id,
                topic=topic,
                direction="down",
                channel="mqtt",
                event_type="command",
                command=command,
                result="pending",
                operator_name=operator,
                payload=payload,
                created_at=now,
            )
        )

    ok, detail = publish(topic, encoded_payload)
    if not ok:
        command_row.status = "publish_error"
        command_row.detail = detail or "MQTT publish failed"
        set_alarm_state(
            db,
            device_id,
            "command_error",
            "critical",
            f"{command} publish failed: {command_row.detail}",
            "backend",
            True,
        )

    db.commit()
    db.refresh(command_row)
    emit_device_events(db, device_id, command_row=command_row)
    return command_row


def retry_pending_command_requests(
    db: Session,
    *,
    publish: Callable[[str, bytes | str], tuple[bool, str]],
    retry_tracker: dict[str, tuple[int, datetime]],
    min_retry_age_seconds: int = 2,
    retry_spacing_seconds: int = 2,
    max_attempts: int = 2,
) -> list[str]:
    now = utcnow()
    eligible_before = naive_utc(now - timedelta(seconds=min_retry_age_seconds))
    retried_device_ids: list[str] = []

    pending_rows = db.scalars(
        select(CommandMessage)
        .where(
            CommandMessage.direction == "down",
            CommandMessage.acked_at.is_(None),
            CommandMessage.status.in_(("pending", "queued", "sent")),
            CommandMessage.created_at <= eligible_before,
        )
        .order_by(CommandMessage.created_at.asc())
    ).all()

    active_keys = {row.msg_id or f"id:{row.id}" for row in pending_rows}
    stale_keys = [key for key in retry_tracker.keys() if key not in active_keys]
    for key in stale_keys:
        retry_tracker.pop(key, None)

    for row in pending_rows:
        tracker_key = row.msg_id or f"id:{row.id}"
        attempts, last_retry_at = retry_tracker.get(tracker_key, (0, datetime.min.replace(tzinfo=timezone.utc)))
        if attempts >= max_attempts:
            continue
        if (now - last_retry_at).total_seconds() < retry_spacing_seconds:
            continue

        if isinstance(row.payload, dict):
            encoded_payload = json.dumps(row.payload, ensure_ascii=False, separators=(",", ":"))
        else:
            encoded_payload = str(row.payload or "")
        if not encoded_payload or not row.topic:
            continue

        ok, detail = publish(row.topic, encoded_payload)
        retry_tracker[tracker_key] = (attempts + 1, now)
        if ok:
            row.detail = f"Retry publish {attempts + 1}/{max_attempts}"
            retried_device_ids.append(row.device_id)
        else:
            row.detail = detail or f"Retry publish {attempts + 1}/{max_attempts} failed"

    if not pending_rows:
        return []

    db.commit()
    for device_id in sorted(set(retried_device_ids)):
        emit_device_events(db, device_id)
    return retried_device_ids


def apply_offline_rules(db: Session, *, commit: bool = True) -> None:
    threshold = naive_utc(utcnow() - timedelta(seconds=settings.offline_seconds))
    devices = db.scalars(select(Device)).all()
    changed: set[str] = set()
    for device in devices:
        if device.last_seen_at is None:
            continue
        if naive_utc(device.last_seen_at) < threshold:
            if device.online:
                changed.add(device.device_id)
            device.online = False
            set_alarm_state(db, device.device_id, "device_offline", "critical", "Device heartbeat timeout", "backend", True)
        else:
            set_alarm_state(db, device.device_id, "device_offline", "critical", "Device online", "backend", False)
    if commit:
        db.commit()
        for device_id in changed:
            emit_device_events(db, device_id)


def apply_command_timeout_rules(db: Session, *, commit: bool = True) -> None:
    threshold = naive_utc(utcnow() - timedelta(seconds=settings.ack_timeout_seconds))
    stale_commands = [
        row
        for row in db.scalars(
            select(CommandMessage).where(
                CommandMessage.direction == "down",
                CommandMessage.acked_at.is_(None),
                CommandMessage.status.in_(("pending", "queued", "sent")),
            )
        ).all()
        if naive_utc(row.created_at) < threshold
    ]

    stale_devices = {command.device_id for command in stale_commands}
    for command in stale_commands:
        command.status = "ack_timeout"
        if not command.detail:
            command.detail = "ACK timeout"
        set_alarm_state(
            db,
            command.device_id,
            "ack_timeout",
            "critical",
            f"{command.command or 'command'} ACK timeout",
            "backend",
            True,
        )

    all_device_ids = db.scalars(select(Device.device_id)).all()
    for device_id in all_device_ids:
        if device_id not in stale_devices:
            set_alarm_state(db, device_id, "ack_timeout", "critical", "ACK recovered", "backend", False)

    if commit:
        db.commit()
        for device_id in stale_devices:
            emit_device_events(db, device_id)


def apply_runtime_rules(db: Session) -> None:
    apply_offline_rules(db, commit=False)
    apply_command_timeout_rules(db, commit=False)
    db.commit()


def device_alarm_count_subquery() -> Select:
    return (
        select(Alarm.device_id, func.count(Alarm.id).label("active_alarm_count"))
        .where(Alarm.active.is_(True))
        .group_by(Alarm.device_id)
        .subquery()
    )
