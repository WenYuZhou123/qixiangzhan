import asyncio
from collections.abc import Iterable
from dataclasses import dataclass
from datetime import datetime, timezone

from fastapi import WebSocket


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


@dataclass
class RealtimeConnection:
    websocket: WebSocket
    user_id: int
    role: str
    device_ids: set[str]


class RealtimeHub:
    def __init__(self) -> None:
        self._loop: asyncio.AbstractEventLoop | None = None
        self._connections: dict[int, RealtimeConnection] = {}

    def attach_loop(self, loop: asyncio.AbstractEventLoop) -> None:
        self._loop = loop

    async def register(self, websocket: WebSocket, *, user_id: int, role: str, device_ids: Iterable[str]) -> None:
        await websocket.accept()
        self._connections[id(websocket)] = RealtimeConnection(
            websocket=websocket,
            user_id=user_id,
            role=role,
            device_ids=set(device_ids),
        )

    def unregister(self, websocket: WebSocket) -> None:
        self._connections.pop(id(websocket), None)

    def publish(self, event_type: str, *, device_id: str | None = None, payload: object = None) -> None:
        if self._loop is None:
            return
        event = {
            "type": event_type,
            "device_id": device_id,
            "timestamp": utcnow().isoformat(),
            "payload": payload,
        }
        self._loop.call_soon_threadsafe(lambda: asyncio.create_task(self._broadcast(event)))

    async def _broadcast(self, event: dict) -> None:
        stale: list[int] = []
        device_id = event.get("device_id") or ""
        for key, connection in list(self._connections.items()):
            if device_id and connection.role != "admin" and device_id not in connection.device_ids:
                continue
            try:
                await connection.websocket.send_json(event)
            except Exception:
                stale.append(key)

        for key in stale:
            self._connections.pop(key, None)


realtime_hub = RealtimeHub()
