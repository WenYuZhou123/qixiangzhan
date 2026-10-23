from datetime import datetime
from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field


class LoginRequest(BaseModel):
    username: str
    password: str


class LoginResponse(BaseModel):
    access_token: str
    refresh_token: str
    token_type: str = "bearer"
    username: str
    display_name: str
    role: str
    access_expires_at: datetime
    refresh_expires_at: datetime


class RefreshRequest(BaseModel):
    refresh_token: str


class LogoutRequest(BaseModel):
    refresh_token: str


class SessionResponse(BaseModel):
    username: str
    display_name: str
    role: str
    is_active: bool
    access_token: str
    refresh_token: str
    access_expires_at: datetime
    refresh_expires_at: datetime


class MeResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    username: str
    display_name: str
    role: str
    is_active: bool
    created_at: datetime
    last_login_at: datetime | None = None


class PadSummaryResponse(BaseModel):
    left_state: str = "closed"
    right_state: str = "closed"
    ready: bool = False
    occupied: bool = False
    mode: str = "auto"


class WeatherCapabilitiesResponse(BaseModel):
    wind: bool = True
    air: bool = True
    rain: bool = True
    pressure: bool = False
    visibility: bool = False


class WeatherSensorStatusResponse(BaseModel):
    wind_online: bool = False
    air_online: bool = False
    rain_online: bool = False
    failure_count: int = 0
    last_ok_tick: int = 0
    last_error: str = ""
    wind_last_tx_hex: str = ""
    wind_last_rx_hex: str = ""
    air_last_frame_hex: str = ""
    l610_state: str = ""


class WeatherSummaryResponse(BaseModel):
    wind_speed: float = 0.0
    wind_direction: float = 0.0
    wind_speed_raw: int = 0
    wind_direction_raw: int = 0
    rain_adc_raw: int = 0
    wind_direction_text: str = ""
    rain_level_text: str = ""
    temperature: float = 0.0
    humidity: float = 0.0
    pressure: float = 0.0
    visibility: float = 0.0
    rain_detected: bool = False
    rain_value: float = 0.0
    pm25: float = 0.0
    pm10: float = 0.0
    co2: float = 0.0
    tvoc: float = 0.0
    ch2o: float = 0.0
    capabilities: WeatherCapabilitiesResponse = Field(default_factory=WeatherCapabilitiesResponse)
    sensor_status: WeatherSensorStatusResponse = Field(default_factory=WeatherSensorStatusResponse)


class ProjectTelemetryResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    device_id: str
    temperature: int = 0
    humidity: int = 0
    speed: int = 0
    direction: str = ""
    uv: str = ""
    raindrop: str = ""
    pm: int = 0
    air_pressure: int = 0
    altitude: int = 0
    pressure: str = ""
    distance: int = 0
    electric: int = 0
    posture: str = ""
    complex: str = ""
    longitude: float = 0.0


class DeviceResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    device_id: str
    display_name: str
    operator_name: str
    ip: str
    online: bool
    rssi: int
    relay1: bool
    relay2: bool
    pad: PadSummaryResponse
    weather: WeatherSummaryResponse
    project: ProjectTelemetryResponse | None = None
    state_text: str
    tick: int
    protocol_profile: str = "relay_v1"
    legacy_status_topic: str = ""
    legacy_command_topic: str = ""
    last_seen_at: datetime | None
    updated_at: datetime
    active_alarm_count: int = 0


class HistoryResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    device_id: str
    msg_id: str | None = None
    topic: str
    direction: str
    channel: str
    event_type: str
    command: str
    result: str
    operator_name: str
    payload: Any
    created_at: datetime


class AlarmResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    alarm_key: str
    device_id: str
    code: str
    severity: str
    message: str
    source: str
    active: bool
    created_at: datetime
    resolved_at: datetime | None = None


class CommandResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    msg_id: str | None = None
    device_id: str
    command: str
    value: str
    direction: str
    status: str
    operator_name: str
    detail: str
    topic: str
    payload: Any
    created_at: datetime
    acked_at: datetime | None = None


class CommandCreateRequest(BaseModel):
    device_id: str
    command: str
    value: str | int | bool | None = None
    operator: str = "qt-desktop"


class CommandCreateResponse(BaseModel):
    msg_id: str
    device_id: str
    command: str
    status: str
    created_at: datetime


class WeatherObservationResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    device_id: str
    msg_id: str | None = None
    topic: str
    observed_at: datetime
    created_at: datetime
    wind_speed: float = 0.0
    wind_direction: float = 0.0
    wind_speed_raw: int = 0
    wind_direction_raw: int = 0
    rain_adc_raw: int = 0
    wind_direction_text: str = ""
    rain_level_text: str = ""
    temperature: float = 0.0
    humidity: float = 0.0
    pressure: float = 0.0
    visibility: float = 0.0
    rain_detected: bool = False
    rain_value: float = 0.0
    pm25: float = 0.0
    pm10: float = 0.0
    co2: float = 0.0
    tvoc: float = 0.0
    ch2o: float = 0.0
    capabilities: Any
    sensor_status: Any = None


class DeviceDiagnosticsResponse(BaseModel):
    device_id: str
    online: bool
    last_seen_at: datetime | None = None
    updated_at: datetime | None = None
    l610: dict[str, Any]
    sensor_status: dict[str, Any]
    last_state: Any = None


class SystemHealthResponse(BaseModel):
    status: str
    time: datetime
    uptime_seconds: float
    api: dict[str, Any]
    mysql: dict[str, Any]
    mqtt: dict[str, Any]
    disk: dict[str, Any]
    devices: dict[str, Any]
    retention: dict[str, Any]


class UserResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    username: str
    display_name: str
    role: str
    is_active: bool
    created_at: datetime
    last_login_at: datetime | None = None
    device_ids: list[str] = []


class UserCreateRequest(BaseModel):
    username: str
    display_name: str
    password: str
    role: str = "operator"
    is_active: bool = True
    device_ids: list[str] = []


class UserUpdateRequest(BaseModel):
    display_name: str | None = None
    password: str | None = None
    role: str | None = None
    is_active: bool | None = None


class DeviceMembershipRequest(BaseModel):
    device_ids: list[str]


class VideoCredentialsResponse(BaseModel):
    device_id: str
    whep_url: str
    username: str
    password: str
    expires_at: datetime
    turn_url: str
    turn_username: str
    turn_password: str


class MediaAuthRequest(BaseModel):
    user: str = ""
    password: str = ""
    action: str = ""
    path: str = ""
    protocol: str = ""


class RealtimeEvent(BaseModel):
    type: str
    device_id: str | None = None
    timestamp: datetime
    payload: Any


class EdgeCapabilities(BaseModel):
    wind: bool
    air: bool
    rain: bool


class EdgeWeather(BaseModel):
    wind_speed_mps: float
    wind_direction_deg: float
    temperature_c: float
    humidity_rh: float
    rain_detected: bool
    rain_value: float
    pm25_ugm3: float
    pm10_ugm3: float
    co2_ppm: float
    tvoc_mg_m3: float
    ch2o_mg_m3: float
    capabilities: EdgeCapabilities


class EdgeFusion(BaseModel):
    risk_level: Literal["normal", "caution", "danger", "unknown"]
    data_valid: bool
    reason: str = ""


class EdgeRelays(BaseModel):
    left_on: bool
    right_on: bool


class EdgeForecastSummary(BaseModel):
    available: bool
    next_three_hour_risk: Literal["normal", "caution", "danger", "unknown"] = "unknown"
    local_correction_applied: bool = False
    hours: list[dict[str, Any]] = Field(default_factory=list, max_length=3)


class EdgeAnalyzeRequest(BaseModel):
    device_id: str = Field(min_length=1, max_length=128)
    snapshot_id: int = Field(ge=0, le=255)
    captured_uptime_ms: int = Field(ge=0)
    status_age_ms: int = Field(ge=0)
    relays: EdgeRelays
    weather: EdgeWeather
    fusion: EdgeFusion
    forecast: EdgeForecastSummary | None = None


class AIModelResult(BaseModel):
    risk_level: Literal["normal", "caution", "danger", "unknown"]
    summary: str = Field(min_length=1, max_length=160)
    reasons: list[str] = Field(default_factory=list, max_length=3)
    recommended_action: Literal["none", "pad_open", "pad_close", "pad_stop", "query_status"]
    requires_confirmation: bool = True


class EdgeAnalyzeResponse(AIModelResult):
    analysis_id: int


class ForecastObservation(BaseModel):
    wind_speed_mps: float = Field(ge=0, le=80)
    wind_direction_deg: float = Field(ge=0, le=360)
    temperature_c: float = Field(ge=-80, le=80)
    humidity_rh: float = Field(ge=0, le=100)
    rain_detected: bool
    wind_online: bool
    rain_online: bool


class EdgeForecastRequest(BaseModel):
    device_id: str = Field(min_length=1, max_length=128)
    location_name: str = Field(default="", max_length=64)
    latitude: float = Field(ge=-90, le=90)
    longitude: float = Field(ge=-180, le=180)
    timezone: str = Field(default="Asia/Shanghai", min_length=1, max_length=64)
    observation: ForecastObservation


class ForecastHourResponse(BaseModel):
    time: str
    temperature_c: float
    humidity_rh: int
    precipitation_probability: int
    precipitation_mm: float
    wind_speed_mps: float
    wind_gust_mps: float
    wind_direction_deg: int
    weather_code: int
    risk_level: Literal["normal", "caution", "danger", "unknown"]


class EdgeForecastResponse(BaseModel):
    source: Literal["open-meteo"] = "open-meteo"
    location_name: str
    timezone: str
    generated_at: datetime
    expires_at: datetime
    next_three_hour_risk: Literal["normal", "caution", "danger", "unknown"]
    local_correction_applied: bool
    hours: list[ForecastHourResponse] = Field(min_length=12, max_length=12)
