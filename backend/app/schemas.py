from datetime import datetime
from typing import Any

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


class RealtimeEvent(BaseModel):
    type: str
    device_id: str | None = None
    timestamp: datetime
    payload: Any
