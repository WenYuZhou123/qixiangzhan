# ESP32 Local Panel

ESP32-S3-Touch-LCD-7 airport local panel. It uses the board RS485 interface at
4800 8N1 and only responds to H743 polling frames.

The 800x480 LVGL UI contains four Chinese pages:

- Overview: link state, temperature, humidity, wind, rain, relay channels, and command result.
- Weather: wind, rainfall, air-quality readings, and explicit unconnected pressure/visibility fields.
- Landing pad: relay state plus open, close, emergency stop, and refresh commands. Open and close require confirmation.
- System: frame sequence, RX/TX, CRC/format/timeout counters, raw readings, and protocol details.

The weather page draws an on-device five-column air-quality reference chart.
PM2.5, PM10, CO2, TVOC, and CH2O are normalized against 35 ug/m3, 50 ug/m3,
1000 ppm, 0.60 mg/m3, and 0.08 mg/m3 respectively. These are visual reference
thresholds only, not a compliance assessment. The wind card has a compass
needle and wind-speed icon, while the landing-pad page animates its two gate
leaves only after a matching successful H743 command result.

Offline sensors and stale H743 state are shown as offline or not updated; no synthetic weather or pad state is displayed.

## Wiring

Connect the ESP board RS485 `A` to the shared H743 RS485 `A`, `B` to `B`, and
connect a signal ground. Keep 120 ohm termination only at physical bus ends.

## Build

Use an ESP-IDF 6.0.2 environment:

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

## Wi-Fi serial diagnostics

The firmware writes diagnostics to the ESP-IDF console on UART0 at 115200 baud.
Use the same port selected for flashing:

```powershell
idf.py -p COMx monitor
```

The `wifi_manager` log reports the configured SSID length, station association,
assigned IPv4 address, and every disconnect reason. Passwords and Edge keys are
never printed. If the board USB connector does not enumerate a COM port, use a
3.3 V USB-to-TTL adapter connected as `adapter RX <- ESP TX0`, `adapter TX ->
ESP RX0`, and `GND -> GND`; do not use the RS485 UART for console diagnostics.

The H743 is the sole RS485 master. ESP uses address `0x7E` and function `0x41`.
Every `STATUS` request includes the fixed 34-byte measurement and relay payload.
ESP responds with `READY`, including an optional pending command: `PAD_OPEN`,
`PAD_CLOSE`, `PAD_STOP`, or `QUERY_STATUS`. H743 executes the command and
returns its result in the next status payload. ESP never transmits spontaneously.

## Cloud AI

The fifth `AI` page fuses the real RS485 wind and rain status before contacting
the backend. Wind below 8 m/s is normal, 8-12 m/s is caution, and rain or wind
at or above 12 m/s is danger. A missing wind or rain sensor disables analysis
and control advice.

Configure the following values through `idf.py menuconfig` under `Cloud AI`:

- Wi-Fi SSID and password.
- Full HTTPS URL ending in `/api/v1/edge/ai/analyze`.
- Edge API key matching backend `QXZ_EDGE_DEVICE_KEY`.
- Device ID, normally `relay_h743_001`.

The ESP uses the certificate bundle for HTTPS. It does not store a cloud-model
API key. AI advice is always shown as an operator proposal and is sent to H743
only after the existing screen confirmation flow succeeds.

### Direct OpenAI-compatible mode

The settings page can also call a cloud model directly. Set the API URL to an
HTTPS URL ending in /chat/completions, set the API key and model name, then
save the AI service. In this mode the ESP sends an OpenAI-compatible request
with a Bearer authorization header and requires a short JSON response. The ESP
starts SNTP after it receives an IPv4 address and waits for valid time before
making the HTTPS request. Direct mode stores the API key in local NVS, so use
a dedicated, quota-limited key and keep backend proxy mode for production.

## Local Settings

The `设置` screen provides the operational configuration surface for Wi-Fi and
the edge AI endpoint. It shows the live RS485, Wi-Fi, Edge API, and AI link
chain, while passwords and Edge Keys remain masked. Unlock the page with the
administrator PIN before editing a field; the initial PIN is `123456` and must
be changed during deployment.

Saved settings use the ESP NVS namespace `panel_cfg` and override menuconfig
defaults after restart. Saving Wi-Fi configuration reconnects the station using
the new values. The screen stores only the Edge Key used to authenticate to
this backend; cloud-provider model API keys remain server-side.
