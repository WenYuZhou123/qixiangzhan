# WebRTC video deployment

Deploy this directory on the public API server. Copy `.env.example` to `.env`, set the media and TURN secrets to the same values used by the FastAPI environment, then start MediaMTX and coturn:

```bash
docker compose -f /opt/qixiangzhan/deploy/video/docker-compose.yml up -d
```

Add `Caddyfile.video.snippet` to the host Caddy configuration, but change the WHEP upstream to the actual MediaMTX address. When Caddy runs in Docker, use the MediaMTX service name on a shared Docker network instead of `127.0.0.1:8889`.

`video.qixiangzhan.online` must be DNS-only and resolve to this server. Open TCP `8554`, TCP/UDP `8189`, TCP/UDP `3478`, and UDP `49160-49200`. Do not place the video hostname behind the Cloudflare HTTP proxy.

On the Jetson, after copying the generated `video-relay.env` to `/etc/qixiangzhan/video-relay.env`, install the staged service template and start it:

```bash
sudo install -m 600 /home/jit/drone_ws/deploy/video/video-relay.env.example /etc/qixiangzhan/video-relay.env
sudo install -m 644 /home/jit/drone_ws/deploy/video/jetson-video-relay.service /etc/systemd/system/ros-video-relay.service
sudo systemctl daemon-reload
sudo systemctl enable --now ros-video-relay
```

Replace the placeholder RTSP password before enabling the service. Check the publisher with `journalctl -u ros-video-relay -f`.
