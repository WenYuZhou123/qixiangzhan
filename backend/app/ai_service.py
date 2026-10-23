import json
import time
from dataclasses import dataclass

import httpx
from pydantic import ValidationError

from .config import settings
from .schemas import AIModelResult, EdgeAnalyzeRequest


SYSTEM_PROMPT = """You are the safety analysis assistant for a local airport landing-pad panel.
Use only the supplied measurements and fusion assessment. Return one JSON object only, with no markdown.
The schema is: {risk_level: normal|caution|danger|unknown, summary: short Chinese text,
reasons: array of at most three short Chinese strings, recommended_action:
none|pad_open|pad_close|pad_stop|query_status, requires_confirmation: true}.
Never claim data is available when capability flags say it is offline. A control recommendation is advisory
only; it must require human confirmation. When a forecast is present, it is a short-term trend only.
Never recommend pad_open from forecast data. For forecast danger recommend only pad_close, pad_stop, or query_status.
Prefer pad_close or pad_stop for dangerous wind or rain.
For the local display, summary must be exactly one of: 天气正常, 风速需要关注, 降雨或强风风险高, 传感器数据不完整.
Each reason must be selected only from: 风速低于8米每秒, 风速为8到12米每秒, 风速达到12米每秒,
检测到降雨, 风场传感器离线, 雨滴传感器离线, 请人工确认后执行.
"""


class AIProviderError(RuntimeError):
    pass


@dataclass(frozen=True)
class AIProviderResult:
    result: AIModelResult
    duration_ms: int


def _chat_completions_url(base_url: str) -> str:
    base_url = base_url.rstrip("/")
    return base_url if base_url.endswith("/chat/completions") else f"{base_url}/chat/completions"


def _json_object(text: str) -> dict:
    text = text.strip()
    if text.startswith("```"):
        text = text.split("\n", 1)[1] if "\n" in text else ""
        if text.rstrip().endswith("```"):
            text = text.rstrip()[:-3]
    start = text.find("{")
    end = text.rfind("}")
    if start < 0 or end <= start:
        raise AIProviderError("model response does not contain JSON")
    try:
        value = json.loads(text[start : end + 1])
    except json.JSONDecodeError as exc:
        raise AIProviderError("model response is not valid JSON") from exc
    if not isinstance(value, dict):
        raise AIProviderError("model JSON must be an object")
    return value


def analyze_with_provider(payload: EdgeAnalyzeRequest) -> AIProviderResult:
    if not settings.llm_api_base_url or not settings.llm_api_key or not settings.llm_model:
        raise AIProviderError("cloud model is not configured")

    started = time.monotonic()
    request_body = {
        "model": settings.llm_model,
        "temperature": 0.1,
        # Qwen thinking responses are much slower and the reasoning text is
        # not useful to this structured, short safety assessment.
        "enable_thinking": False,
        "messages": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": payload.model_dump_json()},
        ],
    }
    try:
        with httpx.Client(timeout=settings.llm_timeout_seconds) as client:
            response = client.post(
                _chat_completions_url(settings.llm_api_base_url),
                headers={"Authorization": f"Bearer {settings.llm_api_key}"},
                json=request_body,
            )
            response.raise_for_status()
            response_json = response.json()
    except (httpx.HTTPError, ValueError) as exc:
        raise AIProviderError("cloud model request failed") from exc

    try:
        content = response_json["choices"][0]["message"]["content"]
        result = AIModelResult.model_validate(_json_object(content))
    except (KeyError, IndexError, TypeError, ValidationError, AIProviderError) as exc:
        raise AIProviderError("cloud model returned an invalid analysis") from exc

    return AIProviderResult(result=result, duration_ms=int((time.monotonic() - started) * 1000))
