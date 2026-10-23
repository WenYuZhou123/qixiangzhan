# L610 MQTT relay

This service accepts the L610's plaintext MQTT connection on port 1883, then bridges only the `relay_h743_001` topics to EMQX Cloud over TLS.

## Deploy on the VPS

1. Create a DNS-only A record: `mqtt.qixiangzhan.online` -> the VPS public IP. Keep the Cloudflare proxy disabled (grey cloud). Cloudflare Tunnel and orange-cloud proxy do not forward raw MQTT TCP 1883.
2. Open inbound TCP 1883 in the cloud security group and host firewall. Do not open any other relay ports.
3. Copy this directory to the VPS, then run:

   ```sh
   cp .env.example .env
   chmod 600 .env
   sh bootstrap.sh
   docker compose logs -f mqtt-relay
   ```

`bootstrap.sh` creates `passwordfile` using the password from `.env`; both `.env` and `passwordfile` are intentionally ignored by Git.

## Expected checks

```sh
docker compose ps
docker compose logs --tail=100 mqtt-relay
```

The log must show the `qixiangzhan-emqx` bridge connected. After powering the L610, it must also show client `l610_relay_001` connected.

The device account ACL allows only:

- subscribe: `device/relay_h743_001/down/cmd`
- publish: `device/relay_h743_001/up/#`

Port 1883 is not encrypted between L610 and the VPS. The distinct password and ACL constrain access, but the cellular-side payload is not confidential.
