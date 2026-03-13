# ESP32-CAM FPV Drone Controller

A self-contained FPV drone controller that runs on an AI-Thinker ESP32-CAM module. It streams live video from the OV2640 camera to a mobile browser and translates touch-joystick inputs into CRSF RC frames sent to a flight controller at 100 Hz.

---

## Hardware

| Component | Detail |
|-----------|--------|
| Board | AI-Thinker ESP32-CAM (OV2640) https://s.click.aliexpress.com/e/_EvJLZDSp |
| Flight controller link | UART2 via CRSF protocol |
| TX pin (ESP32 → FC) | GPIO 14 (SD_CLK — not a strapping pin, safe when SD unused) |
| RX pin (FC → ESP32) | GPIO 13 (SD_DATA3 — telemetry input, optional) |
| Power | 5 V on the CAM connector |

All pins are configurable at runtime through the web config page and stored in NVS flash.

---

## Network

| Parameter | Value |
|-----------|-------|
| Mode | WiFi Access Point |
| SSID | `ESPCam` |
| Password | `ESPCam` |
| IP address | `192.168.0.1` |

Connect your phone or laptop to the `ESPCam` network, then open `http://192.168.0.1/` in a browser.

---

## Web Interface

### Controller page — `http://192.168.0.1/`

Responsive layout that adapts to portrait and landscape orientations.

**Landscape**: left stick | live video | right stick, with arm/FM controls below the video.

**Portrait**: live video fills the top, both sticks sit side-by-side below it, arm/FM controls at the bottom.

| Control | Axis / Action |
|---------|--------------|
| Left stick — vertical | Throttle (up = positive) |
| Left stick — horizontal | Yaw |
| Right stick — horizontal | Roll |
| Right stick — vertical | Pitch (up = positive) |
| ARM button | Toggles CH5: low = disarmed, high = armed |
| STB / ACRO / AUTO | 3-position flight mode on CH6: low / mid / high |

Both sticks snap back to center (1500 µs) on release. The throttle stick also snaps to center — use the flight controller's hover-throttle or altitude-hold mode if you want hands-off level flight.

**Link timeout safety**: if no WebSocket message is received for 500 ms (e.g. browser tab closed, phone screen locked), all channels immediately revert to safe state — sticks to center, arm to disarmed.

### Video stream — `http://192.168.0.1:81/stream`

MJPEG stream served from a dedicated ESP-IDF HTTP server on port 81. The `<img>` tag on the controller page points to this URL directly. If the camera fails to initialise at boot, the stream is simply unavailable but the rest of the controller still works.

### Config page — `http://192.168.0.1/config`

Allows changing:
- TX / RX GPIO pins for the CRSF serial link
- Camera resolution (QQVGA 160×120 / QVGA 320×240 / VGA 640×480)
- CRSF channel numbers for roll, pitch, throttle, yaw, arm switch, and flight mode

Settings are written to NVS flash and take effect after the device reboots (reboot is triggered automatically on save).

---

## CRSF Output

The ESP32 generates CRSF RC frames from scratch — it is the packet source, not a forwarder. Frames are sent at **100 Hz** on UART2 using the `AlfredoCRSF` library.

Channel value mapping:

| Input value | CRSF value | Meaning |
|-------------|-----------|---------|
| −100 | 191 | minimum (1000 µs) |
| 0 | 992 | center (1500 µs) |
| +100 | 1792 | maximum (2000 µs) |

The CRSF task runs on **core 0** (FreeRTOS, priority 2). The web server and camera run on core 1.

---

## Dependencies

Install via Arduino IDE Library Manager or by copying into your `libraries/` folder:

| Library | Source |
|---------|--------|
| AlfredoCRSF | `/Users/vostapiv/Drones/AlfredoCRSF/` (sibling project) |
| ESPAsyncWebServer | ESP Async WebServer |
| AsyncTCP | AsyncTCP |
| ArduinoJson | ArduinoJson |

Board package: **esp32 by Espressif** — select board **AI Thinker ESP32-CAM**.

---

## Project Structure

```
ESP32cam/
├── ESP32cam.ino        — Entry point: WiFi AP, setup(), loop()
├── config.h            — Config struct, NVS load/save (inline to avoid ODR issues)
├── camera_stream.h     — Forward declarations (no conflicting headers)
├── camera_stream.cpp   — OV2640 init + MJPEG server (isolated TU)
├── crsf_output.h       — CRSF frame builder + FreeRTOS task
├── web_server.h        — AsyncWebServer + WebSocket handler
├── ui_main.h           — PROGMEM HTML: controller UI
└── ui_config.h         — PROGMEM HTML: config page
```

`camera_stream.cpp` is a separate translation unit to avoid a macro name conflict: both `esp_http_server.h` (used by the camera library) and `ESPAsyncWebServer.h` define `HTTP_GET`, `HTTP_POST`, etc. Isolating the camera code in its own `.cpp` keeps these headers from ever appearing in the same compilation unit.

---

## Flashing

1. Open `ESP32cam.ino` in Arduino IDE.
2. Select **Tools → Board → AI Thinker ESP32-CAM**.
3. Set **Tools → Partition Scheme → Huge APP** (more flash for program, no OTA).
4. Connect a USB-UART adapter: GND→GND, 5V→5V (not 3.3V!), GPIO0→GND (boot mode), U0TX→RX, U0RX→TX.
5. Press reset on the board, then upload.
6. After flashing, disconnect GPIO0 from GND and press reset to boot normally.
7. Open Serial Monitor at **115200 baud** to confirm startup output.

---

## Serial Monitor Output (normal boot)

```
╔══════════════════════════════════╗
║   ESP32-CAM Drone Controller     ║
╚══════════════════════════════════╝
  Chip: ESP32-S  rev3  cores=2
  Flash: 4 MB
  Free heap: ...
  PSRAM: found  (...)

[CFG]  Loading NVS config...
[CFG]  TX=14  RX=13  Roll=CH1  Pitch=CH2  Thr=CH3  Yaw=CH4  Arm=CH5  FM=CH6
[WiFi] Starting Access Point...
[WiFi] AP ready  SSID=ESPCam    IP=192.168.0.1
[CAM]  Initialising OV2640...
[CAM]  Starting stream server on :81...
[WEB]  Starting HTTP server on :80...
[CRSF] Initialising UART...
[CRSF] Starting 100 Hz output task on core 0...

════════════════════════════════════
  Connect to WiFi: ESPCam
  Password:        ESPCam
  Open browser:    http://192.168.0.1/
════════════════════════════════════
```

---

## Wiring to Flight Controller

Connect UART2 TX (GPIO 14 by default) to the FC's CRSF/ELRS receiver UART RX pad. If your FC requires bidirectional CRSF (for telemetry), also connect GPIO 13 to the FC's UART TX pad. Most FC configurators (Betaflight, iNav) auto-detect CRSF at 420000 baud.
