import threading
import time
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone

import httpx

from .config import settings
from .schemas import EdgeForecastRequest, EdgeForecastResponse, ForecastHourResponse

OPEN_METEO_URL = "https://api.open-meteo.com/v1/forecast"
_cache_lock = threading.Lock()
_cache: dict[tuple[float, float, str], tuple[float, dict]] = {}
_risk_rank = {"normal": 0, "caution": 1, "danger": 2, "unknown": 3}


class ForecastProviderError(RuntimeError):
    pass


def _risk(wind: float, gust: float, probability: int, precipitation: float) -> str:
    if wind >= 12.0 or gust >= 15.0 or precipitation >= 0.2 or probability >= 70:
        return "danger"
    if wind >= 8.0 or gust >= 12.0 or probability >= 40:
        return "caution"
    return "normal"


def _cached_open_meteo(latitude: float, longitude: float, timezone_name: str) -> dict:
    key = (round(latitude, 4), round(longitude, 4), timezone_name)
    now = time.monotonic()
    with _cache_lock:
        cached = _cache.get(key)
        if cached is not None and now < cached[0]:
            return cached[1]

    params = {
        "latitude": latitude,
        "longitude": longitude,
        "timezone": timezone_name,
        "forecast_hours": 12,
        "hourly": "temperature_2m,relative_humidity_2m,precipitation_probability,precipitation,weather_code,wind_speed_10m,wind_direction_10m,wind_gusts_10m",
    }
    try:
        with httpx.Client(timeout=settings.forecast_timeout_seconds) as client:
            response = client.get(OPEN_METEO_URL, params=params)
            response.raise_for_status()
            payload = response.json()
    except (httpx.HTTPError, ValueError) as exc:
        raise ForecastProviderError("Open-Meteo request failed") from exc

    hourly = payload.get("hourly")
    required = ("time", "temperature_2m", "relative_humidity_2m", "precipitation_probability", "precipitation", "weather_code", "wind_speed_10m", "wind_direction_10m", "wind_gusts_10m")
    if not isinstance(hourly, dict) or any(not isinstance(hourly.get(name), list) or len(hourly[name]) < 12 for name in required):
        raise ForecastProviderError("Open-Meteo returned incomplete hourly data")
    with _cache_lock:
        _cache[key] = (now + settings.forecast_cache_seconds, payload)
    return payload


def build_forecast(request: EdgeForecastRequest) -> EdgeForecastResponse:
    payload = _cached_open_meteo(request.latitude, request.longitude, request.timezone)
    hourly = payload["hourly"]
    hours: list[ForecastHourResponse] = []
    correction_applied = request.observation.wind_online or request.observation.rain_online
    for index in range(12):
        raw_wind = float(hourly["wind_speed_10m"][index]) / 3.6
        raw_gust = float(hourly["wind_gusts_10m"][index]) / 3.6
        local_factor = (1.0, 0.5, 0.25)[index] if index < 3 else 0.0
        corrected_wind = max(raw_wind, request.observation.wind_speed_mps * local_factor) if request.observation.wind_online else raw_wind
        probability = int(hourly["precipitation_probability"][index])
        precipitation = float(hourly["precipitation"][index])
        if index == 0 and request.observation.rain_online and request.observation.rain_detected:
            probability = max(probability, 100)
            precipitation = max(precipitation, 0.2)
        hours.append(ForecastHourResponse(
            time=str(hourly["time"][index]),
            temperature_c=float(hourly["temperature_2m"][index]),
            humidity_rh=int(hourly["relative_humidity_2m"][index]),
            precipitation_probability=probability,
            precipitation_mm=precipitation,
            wind_speed_mps=round(corrected_wind, 1),
            wind_gust_mps=round(max(raw_gust, corrected_wind), 1),
            wind_direction_deg=int(hourly["wind_direction_10m"][index]),
            weather_code=int(hourly["weather_code"][index]),
            risk_level=_risk(corrected_wind, max(raw_gust, corrected_wind), probability, precipitation),
        ))
    next_risk = max((hour.risk_level for hour in hours[:3]), key=lambda level: _risk_rank[level])
    generated_at = datetime.now(timezone.utc)
    return EdgeForecastResponse(
        location_name=request.location_name,
        timezone=request.timezone,
        generated_at=generated_at,
        expires_at=generated_at + timedelta(seconds=settings.forecast_cache_seconds),
        next_three_hour_risk=next_risk,
        local_correction_applied=correction_applied,
        hours=hours,
    )
