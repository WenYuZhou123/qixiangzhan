from sqlalchemy import inspect, text

from .database import Base, engine


def _column_names(table_name: str) -> set[str]:
    inspector = inspect(engine)
    try:
        return {column["name"] for column in inspector.get_columns(table_name)}
    except Exception:
        return set()


def _index_names(table_name: str) -> set[str]:
    inspector = inspect(engine)
    try:
        return {index["name"] for index in inspector.get_indexes(table_name)}
    except Exception:
        return set()


def ensure_runtime_schema() -> None:
    Base.metadata.create_all(bind=engine)

    user_columns = _column_names("users")
    device_columns = _column_names("devices")

    statements: list[str] = []
    if "role" not in user_columns:
        statements.append("ALTER TABLE users ADD COLUMN role VARCHAR(32) NOT NULL DEFAULT 'operator'")
    if "last_login_at" not in user_columns:
        statements.append("ALTER TABLE users ADD COLUMN last_login_at DATETIME NULL")
    if "password_changed_at" not in user_columns:
        statements.append("ALTER TABLE users ADD COLUMN password_changed_at DATETIME NULL")

    if "protocol_profile" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN protocol_profile VARCHAR(64) NOT NULL DEFAULT 'relay_v1'")
    if "legacy_status_topic" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN legacy_status_topic VARCHAR(256) NOT NULL DEFAULT ''")
    if "legacy_command_topic" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN legacy_command_topic VARCHAR(256) NOT NULL DEFAULT ''")
    if "pad_left_state" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN pad_left_state VARCHAR(32) NOT NULL DEFAULT 'closed'")
    if "pad_right_state" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN pad_right_state VARCHAR(32) NOT NULL DEFAULT 'closed'")
    if "pad_ready" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN pad_ready BOOLEAN NOT NULL DEFAULT 0")
    if "pad_occupied" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN pad_occupied BOOLEAN NOT NULL DEFAULT 0")
    if "pad_mode" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN pad_mode VARCHAR(32) NOT NULL DEFAULT 'auto'")
    if "weather_wind_speed" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN weather_wind_speed FLOAT NOT NULL DEFAULT 0")
    if "weather_wind_direction" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN weather_wind_direction FLOAT NOT NULL DEFAULT 0")
    if "weather_temperature" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN weather_temperature FLOAT NOT NULL DEFAULT 0")
    if "weather_humidity" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN weather_humidity FLOAT NOT NULL DEFAULT 0")
    if "weather_pressure" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN weather_pressure FLOAT NOT NULL DEFAULT 0")
    if "weather_visibility" not in device_columns:
        statements.append("ALTER TABLE devices ADD COLUMN weather_visibility FLOAT NOT NULL DEFAULT 0")

    with engine.begin() as conn:
        for statement in statements:
            conn.execute(text(statement))

        user_indexes = _index_names("users")
        if "ix_users_role" not in user_indexes:
            conn.execute(text("CREATE INDEX ix_users_role ON users (role)"))

        device_indexes = _index_names("devices")
        if "ix_devices_protocol_profile" not in device_indexes:
            conn.execute(text("CREATE INDEX ix_devices_protocol_profile ON devices (protocol_profile)"))
