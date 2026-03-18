from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    api_host: str = "0.0.0.0"
    api_port: int = 8000
    public_api_base: str = "https://api.qixiangzhan.online/api/v1"
    database_url: str = "mysql+pymysql://qixiang_app:change-me@127.0.0.1:3306/qixiangzhan?charset=utf8mb4"
    mqtt_host: str = "rc11adc1.ala.cn-hangzhou.emqxsl.cn"
    mqtt_port: int = 8883
    mqtt_username: str = "h743"
    mqtt_password: str = "123456"
    mqtt_use_tls: bool = True
    mqtt_keepalive: int = 60
    admin_username: str = "admin"
    admin_password: str = "admin123"
    admin_display_name: str = "Platform Admin"
    token_secret: str = "qixiangzhan-dev-secret"
    token_issuer: str = "qixiangzhan-backend"
    access_token_minutes: int = 15
    refresh_token_days: int = 30
    offline_seconds: int = 90
    ack_timeout_seconds: int = 15
    ws_heartbeat_seconds: int = 20
    json_logs: bool = False

    model_config = SettingsConfigDict(env_prefix="QXZ_", env_file=".env", extra="ignore")


settings = Settings()
