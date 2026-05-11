"""add jetson edge observation and event tables

Revision ID: 0001_jetson_edge_tables
Revises:
Create Date: 2026-05-09
"""
from alembic import op
import sqlalchemy as sa

revision = "0001_jetson_edge_tables"
down_revision = None
branch_labels = None
depends_on = None


def upgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)
    tables = set(inspector.get_table_names())

    if "devices" not in tables:
        from app import models  # noqa: F401
        from app.database import Base

        Base.metadata.create_all(bind=bind)
        return

    device_columns = {column["name"] for column in inspector.get_columns("devices")}
    if "weather_sensor_status" not in device_columns:
        op.add_column("devices", sa.Column("weather_sensor_status", sa.JSON(), nullable=True))

    if "weather_observations" not in tables:
        op.create_table(
            "weather_observations",
            sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
            sa.Column("device_id", sa.String(length=128), nullable=False),
            sa.Column("msg_id", sa.String(length=128), nullable=True),
            sa.Column("topic", sa.String(length=256), nullable=False),
            sa.Column("observed_at", sa.DateTime(timezone=True), nullable=False),
            sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
            sa.Column("wind_speed", sa.Float(), nullable=False, server_default="0"),
            sa.Column("wind_direction", sa.Float(), nullable=False, server_default="0"),
            sa.Column("wind_speed_raw", sa.Integer(), nullable=False, server_default="0"),
            sa.Column("wind_direction_raw", sa.Integer(), nullable=False, server_default="0"),
            sa.Column("rain_adc_raw", sa.Integer(), nullable=False, server_default="0"),
            sa.Column("wind_direction_text", sa.String(length=32), nullable=False, server_default=""),
            sa.Column("rain_level_text", sa.String(length=32), nullable=False, server_default=""),
            sa.Column("temperature", sa.Float(), nullable=False, server_default="0"),
            sa.Column("humidity", sa.Float(), nullable=False, server_default="0"),
            sa.Column("pressure", sa.Float(), nullable=False, server_default="0"),
            sa.Column("visibility", sa.Float(), nullable=False, server_default="0"),
            sa.Column("rain_detected", sa.Boolean(), nullable=False, server_default=sa.false()),
            sa.Column("rain_value", sa.Float(), nullable=False, server_default="0"),
            sa.Column("pm25", sa.Float(), nullable=False, server_default="0"),
            sa.Column("pm10", sa.Float(), nullable=False, server_default="0"),
            sa.Column("co2", sa.Float(), nullable=False, server_default="0"),
            sa.Column("tvoc", sa.Float(), nullable=False, server_default="0"),
            sa.Column("ch2o", sa.Float(), nullable=False, server_default="0"),
            sa.Column("capabilities", sa.JSON(), nullable=False),
            sa.Column("sensor_status", sa.JSON(), nullable=True),
            sa.Column("payload", sa.JSON(), nullable=False),
        )
        op.create_index("ix_weather_observations_device_observed_at", "weather_observations", ["device_id", "observed_at"])
        op.create_index("ix_weather_observations_device_created_at", "weather_observations", ["device_id", "created_at"])
        op.create_index("ix_weather_observations_device_id", "weather_observations", ["device_id"])
        op.create_index("ix_weather_observations_msg_id", "weather_observations", ["msg_id"])
        op.create_index("ix_weather_observations_topic", "weather_observations", ["topic"])
        op.create_index("ix_weather_observations_observed_at", "weather_observations", ["observed_at"])
        op.create_index("ix_weather_observations_created_at", "weather_observations", ["created_at"])

    if "system_events" not in tables:
        op.create_table(
            "system_events",
            sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
            sa.Column("event_type", sa.String(length=64), nullable=False),
            sa.Column("severity", sa.String(length=16), nullable=False, server_default="info"),
            sa.Column("source", sa.String(length=64), nullable=False, server_default="backend"),
            sa.Column("message", sa.Text(), nullable=False),
            sa.Column("payload", sa.JSON(), nullable=False),
            sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
        )
        op.create_index("ix_system_events_event_type_created_at", "system_events", ["event_type", "created_at"])
        op.create_index("ix_system_events_severity_created_at", "system_events", ["severity", "created_at"])
        op.create_index("ix_system_events_event_type", "system_events", ["event_type"])
        op.create_index("ix_system_events_severity", "system_events", ["severity"])
        op.create_index("ix_system_events_created_at", "system_events", ["created_at"])


def downgrade() -> None:
    op.drop_index("ix_system_events_created_at", table_name="system_events")
    op.drop_index("ix_system_events_severity", table_name="system_events")
    op.drop_index("ix_system_events_event_type", table_name="system_events")
    op.drop_index("ix_system_events_severity_created_at", table_name="system_events")
    op.drop_index("ix_system_events_event_type_created_at", table_name="system_events")
    op.drop_table("system_events")
    op.drop_index("ix_weather_observations_created_at", table_name="weather_observations")
    op.drop_index("ix_weather_observations_observed_at", table_name="weather_observations")
    op.drop_index("ix_weather_observations_topic", table_name="weather_observations")
    op.drop_index("ix_weather_observations_msg_id", table_name="weather_observations")
    op.drop_index("ix_weather_observations_device_id", table_name="weather_observations")
    op.drop_index("ix_weather_observations_device_created_at", table_name="weather_observations")
    op.drop_index("ix_weather_observations_device_observed_at", table_name="weather_observations")
    op.drop_table("weather_observations")
    op.drop_column("devices", "weather_sensor_status")
