from datetime import datetime

from sqlalchemy import JSON, Boolean, DateTime, Float, ForeignKey, Index, Integer, String, Text
from sqlalchemy.orm import Mapped, mapped_column

from .database import Base


class User(Base):
    __tablename__ = "users"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    username: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    display_name: Mapped[str] = mapped_column(String(128))
    password_hash: Mapped[str] = mapped_column(String(128))
    role: Mapped[str] = mapped_column(String(32), default="operator", index=True)
    is_active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow)
    last_login_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    password_changed_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)


class Device(Base):
    __tablename__ = "devices"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(String(128), unique=True, index=True)
    display_name: Mapped[str] = mapped_column(String(128), default="")
    operator_name: Mapped[str] = mapped_column(String(128), default="")
    ip: Mapped[str] = mapped_column(String(64), default="")
    online: Mapped[bool] = mapped_column(Boolean, default=False)
    rssi: Mapped[int] = mapped_column(Integer, default=0)
    relay1: Mapped[bool] = mapped_column(Boolean, default=False)
    relay2: Mapped[bool] = mapped_column(Boolean, default=False)
    pad_left_state: Mapped[str] = mapped_column(String(32), default="closed")
    pad_right_state: Mapped[str] = mapped_column(String(32), default="closed")
    pad_ready: Mapped[bool] = mapped_column(Boolean, default=False)
    pad_occupied: Mapped[bool] = mapped_column(Boolean, default=False)
    pad_mode: Mapped[str] = mapped_column(String(32), default="auto")
    weather_wind_speed: Mapped[float] = mapped_column(Float, default=0.0)
    weather_wind_direction: Mapped[float] = mapped_column(Float, default=0.0)
    weather_wind_speed_raw: Mapped[int] = mapped_column(Integer, default=0)
    weather_wind_direction_raw: Mapped[int] = mapped_column(Integer, default=0)
    weather_rain_adc_raw: Mapped[int] = mapped_column(Integer, default=0)
    weather_wind_direction_text: Mapped[str] = mapped_column(String(32), default="")
    weather_rain_level_text: Mapped[str] = mapped_column(String(32), default="")
    weather_temperature: Mapped[float] = mapped_column(Float, default=0.0)
    weather_humidity: Mapped[float] = mapped_column(Float, default=0.0)
    weather_pressure: Mapped[float] = mapped_column(Float, default=0.0)
    weather_visibility: Mapped[float] = mapped_column(Float, default=0.0)
    weather_capability_wind: Mapped[bool] = mapped_column(Boolean, default=True)
    weather_capability_air: Mapped[bool] = mapped_column(Boolean, default=True)
    weather_capability_rain: Mapped[bool] = mapped_column(Boolean, default=True)
    weather_capability_pressure: Mapped[bool] = mapped_column(Boolean, default=False)
    weather_capability_visibility: Mapped[bool] = mapped_column(Boolean, default=False)
    weather_rain_detected: Mapped[bool] = mapped_column(Boolean, default=False)
    weather_rain_value: Mapped[float] = mapped_column(Float, default=0.0)
    weather_pm25: Mapped[float] = mapped_column(Float, default=0.0)
    weather_pm10: Mapped[float] = mapped_column(Float, default=0.0)
    weather_co2: Mapped[float] = mapped_column(Float, default=0.0)
    weather_tvoc: Mapped[float] = mapped_column(Float, default=0.0)
    weather_ch2o: Mapped[float] = mapped_column(Float, default=0.0)
    weather_sensor_status: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    state_text: Mapped[str] = mapped_column(String(256), default="")
    tick: Mapped[int] = mapped_column(Integer, default=0)
    protocol_profile: Mapped[str] = mapped_column(String(64), default="relay_v1", index=True)
    legacy_status_topic: Mapped[str] = mapped_column(String(256), default="")
    legacy_command_topic: Mapped[str] = mapped_column(String(256), default="")
    last_seen_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow)


class DeviceMembership(Base):
    __tablename__ = "device_memberships"
    __table_args__ = (
        Index("ix_device_memberships_user_device", "user_id", "device_id", unique=True),
        Index("ix_device_memberships_device_id", "device_id"),
    )

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    user_id: Mapped[int] = mapped_column(ForeignKey("users.id", ondelete="CASCADE"), index=True)
    device_id: Mapped[str] = mapped_column(String(128))
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow)


class RefreshToken(Base):
    __tablename__ = "refresh_tokens"
    __table_args__ = (
        Index("ix_refresh_tokens_user_active", "user_id", "revoked_at"),
        Index("ix_refresh_tokens_expires_at", "expires_at"),
    )

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    user_id: Mapped[int] = mapped_column(ForeignKey("users.id", ondelete="CASCADE"), index=True)
    token_hash: Mapped[str] = mapped_column(String(128), unique=True, index=True)
    label: Mapped[str] = mapped_column(String(128), default="")
    expires_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow)
    last_used_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    revoked_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)


class DeviceLastState(Base):
    __tablename__ = "device_last_state"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(String(128), unique=True, index=True)
    payload: Mapped[dict] = mapped_column(JSON)
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow)


class WeatherObservation(Base):
    __tablename__ = "weather_observations"
    __table_args__ = (
        Index("ix_weather_observations_device_observed_at", "device_id", "observed_at"),
        Index("ix_weather_observations_device_created_at", "device_id", "created_at"),
    )

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(String(128), index=True)
    msg_id: Mapped[str | None] = mapped_column(String(128), index=True, nullable=True)
    topic: Mapped[str] = mapped_column(String(256), index=True)
    observed_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), index=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow, index=True)
    wind_speed: Mapped[float] = mapped_column(Float, default=0.0)
    wind_direction: Mapped[float] = mapped_column(Float, default=0.0)
    wind_speed_raw: Mapped[int] = mapped_column(Integer, default=0)
    wind_direction_raw: Mapped[int] = mapped_column(Integer, default=0)
    rain_adc_raw: Mapped[int] = mapped_column(Integer, default=0)
    wind_direction_text: Mapped[str] = mapped_column(String(32), default="")
    rain_level_text: Mapped[str] = mapped_column(String(32), default="")
    temperature: Mapped[float] = mapped_column(Float, default=0.0)
    humidity: Mapped[float] = mapped_column(Float, default=0.0)
    pressure: Mapped[float] = mapped_column(Float, default=0.0)
    visibility: Mapped[float] = mapped_column(Float, default=0.0)
    rain_detected: Mapped[bool] = mapped_column(Boolean, default=False)
    rain_value: Mapped[float] = mapped_column(Float, default=0.0)
    pm25: Mapped[float] = mapped_column(Float, default=0.0)
    pm10: Mapped[float] = mapped_column(Float, default=0.0)
    co2: Mapped[float] = mapped_column(Float, default=0.0)
    tvoc: Mapped[float] = mapped_column(Float, default=0.0)
    ch2o: Mapped[float] = mapped_column(Float, default=0.0)
    capabilities: Mapped[dict] = mapped_column(JSON)
    sensor_status: Mapped[dict | None] = mapped_column(JSON, nullable=True)
    payload: Mapped[dict | str] = mapped_column(JSON)


class SystemEvent(Base):
    __tablename__ = "system_events"
    __table_args__ = (
        Index("ix_system_events_event_type_created_at", "event_type", "created_at"),
        Index("ix_system_events_severity_created_at", "severity", "created_at"),
    )

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    event_type: Mapped[str] = mapped_column(String(64), index=True)
    severity: Mapped[str] = mapped_column(String(16), default="info")
    source: Mapped[str] = mapped_column(String(64), default="backend")
    message: Mapped[str] = mapped_column(Text, default="")
    payload: Mapped[dict | str] = mapped_column(JSON, default=dict)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow, index=True)


class TelemetryMessage(Base):
    __tablename__ = "telemetry_messages"
    __table_args__ = (
        Index("ix_telemetry_messages_device_created_at", "device_id", "created_at"),
        Index("ix_telemetry_messages_device_topic_created_at", "device_id", "topic", "created_at"),
    )

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(String(128), index=True)
    msg_id: Mapped[str | None] = mapped_column(String(128), index=True, nullable=True)
    topic: Mapped[str] = mapped_column(String(256), index=True)
    direction: Mapped[str] = mapped_column(String(16))
    channel: Mapped[str] = mapped_column(String(16), default="mqtt")
    event_type: Mapped[str] = mapped_column(String(32), default="")
    command: Mapped[str] = mapped_column(String(64), default="")
    result: Mapped[str] = mapped_column(String(32), default="")
    operator_name: Mapped[str] = mapped_column(String(128), default="")
    payload: Mapped[dict | str] = mapped_column(JSON)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow, index=True)


class CommandMessage(Base):
    __tablename__ = "command_messages"
    __table_args__ = (
        Index("ix_command_messages_device_created_at", "device_id", "created_at"),
        Index("ix_command_messages_device_status", "device_id", "status"),
    )

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    msg_id: Mapped[str | None] = mapped_column(String(128), unique=True, nullable=True)
    device_id: Mapped[str] = mapped_column(String(128), index=True)
    command: Mapped[str] = mapped_column(String(64))
    value: Mapped[str] = mapped_column(String(64), default="")
    direction: Mapped[str] = mapped_column(String(16), default="down")
    status: Mapped[str] = mapped_column(String(32), default="pending")
    operator_name: Mapped[str] = mapped_column(String(128), default="")
    detail: Mapped[str] = mapped_column(Text, default="")
    topic: Mapped[str] = mapped_column(String(256), default="")
    payload: Mapped[dict | str] = mapped_column(JSON)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow, index=True)
    acked_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)


class Alarm(Base):
    __tablename__ = "alarms"
    __table_args__ = (
        Index("ix_alarms_device_active", "device_id", "active"),
        Index("ix_alarms_device_created_at", "device_id", "created_at"),
    )

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    alarm_key: Mapped[str] = mapped_column(String(192), unique=True, index=True)
    device_id: Mapped[str] = mapped_column(String(128), index=True)
    code: Mapped[str] = mapped_column(String(64))
    severity: Mapped[str] = mapped_column(String(32), default="warning")
    message: Mapped[str] = mapped_column(Text)
    source: Mapped[str] = mapped_column(String(64), default="backend")
    active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow, index=True)
    resolved_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)


class AuditLog(Base):
    __tablename__ = "audit_logs"
    __table_args__ = (
        Index("ix_audit_logs_user_created_at", "user_id", "created_at"),
        Index("ix_audit_logs_device_created_at", "device_id", "created_at"),
        Index("ix_audit_logs_event_type_created_at", "event_type", "created_at"),
    )

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    user_id: Mapped[int | None] = mapped_column(ForeignKey("users.id", ondelete="SET NULL"), nullable=True, index=True)
    username: Mapped[str] = mapped_column(String(64), default="", index=True)
    role: Mapped[str] = mapped_column(String(32), default="")
    device_id: Mapped[str] = mapped_column(String(128), default="", index=True)
    event_type: Mapped[str] = mapped_column(String(64), index=True)
    severity: Mapped[str] = mapped_column(String(16), default="info")
    remote_addr: Mapped[str] = mapped_column(String(128), default="")
    user_agent: Mapped[str] = mapped_column(String(256), default="")
    message: Mapped[str] = mapped_column(Text, default="")
    payload: Mapped[dict | str] = mapped_column(JSON, default=dict)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow, index=True)
