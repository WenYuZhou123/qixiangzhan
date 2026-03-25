import ssl
import logging
import os
import socket

import paho.mqtt.client as mqtt

from .config import settings
from .database import SessionLocal
from .services import record_message

logger = logging.getLogger(__name__)
MQTT_PUBLISH_WAIT_TIMEOUT_SEC = 2.0


class EmqxBridge:
    def __init__(self) -> None:
        client_id = f"qixiangzhan-backend-{socket.gethostname()}-{os.getpid()}"
        self._client = mqtt.Client(client_id=client_id, clean_session=True, protocol=mqtt.MQTTv311)
        self._connected = False
        self._client.username_pw_set(settings.mqtt_username, settings.mqtt_password)
        if settings.mqtt_use_tls:
            self._client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
        self._client.reconnect_delay_set(min_delay=1, max_delay=30)
        self._client.enable_logger(logger)
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.on_message = self._on_message

    @property
    def connected(self) -> bool:
        try:
            return self._connected and self._client.is_connected()
        except Exception:
            return self._connected

    def start(self) -> None:
        logger.info("Starting MQTT bridge -> %s:%s tls=%s", settings.mqtt_host, settings.mqtt_port, settings.mqtt_use_tls)
        self._client.connect(settings.mqtt_host, settings.mqtt_port, settings.mqtt_keepalive)
        self._client.loop_start()

    def stop(self) -> None:
        self._client.loop_stop()
        self._client.disconnect()

    def publish(self, topic: str, payload: bytes | str, qos: int = 1, retain: bool = False) -> tuple[bool, str]:
        if not self.connected:
            logger.warning("MQTT bridge publish while disconnected, attempting reconnect")
            try:
                self._client.reconnect()
            except Exception as exc:  # pragma: no cover
                logger.warning("MQTT reconnect failed: %s", exc)
                return False, str(exc)

        message = payload.encode("utf-8") if isinstance(payload, str) else payload
        info = self._client.publish(topic, message, qos=qos, retain=retain)
        if info.rc == mqtt.MQTT_ERR_SUCCESS:
            try:
                info.wait_for_publish(timeout=MQTT_PUBLISH_WAIT_TIMEOUT_SEC)
            except Exception as exc:  # pragma: no cover
                logger.warning("MQTT wait_for_publish failed: %s", exc)
                return False, str(exc)

            if info.is_published():
                return True, ""
            return False, "MQTT publish timeout"
        return False, mqtt.error_string(info.rc)

    def _on_connect(self, client: mqtt.Client, userdata: object, flags: dict, rc: int, properties: object = None) -> None:
        self._connected = rc == 0
        if rc != 0:
            logger.error("MQTT bridge connect failed: rc=%s", rc)
            return

        logger.info("MQTT bridge connected")
        topics = [
            "device/+/down/cmd",
            "device/+/up/status",
            "device/+/up/ack",
            "device/+/up/event",
            "device/+/up/online",
            "device/+/status",
        ]
        for topic in topics:
            client.subscribe(topic, qos=1)
            logger.info("MQTT bridge subscribed: %s", topic)

    def _on_disconnect(self, client: mqtt.Client, userdata: object, rc: int, properties: object = None) -> None:
        self._connected = False
        if rc == 0:
            logger.info("MQTT bridge disconnected")
            return
        logger.warning("MQTT bridge unexpected disconnect: rc=%s", rc)

    def _on_message(self, client: mqtt.Client, userdata: object, msg: mqtt.MQTTMessage) -> None:
        try:
            with SessionLocal() as session:
                record_message(session, msg.topic, msg.payload)
        except Exception:  # pragma: no cover
            logger.exception("MQTT bridge failed to record message: topic=%s", msg.topic)
