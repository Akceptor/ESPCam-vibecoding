# Product Requirements Document — ESP32-CAM FPV Drone Controller

## 1. Overview

A self-contained FPV ground station that fits in the palm of a hand. The operator connects their phone to a WiFi access point hosted by the ESP32-CAM, opens a browser, and controls the drone with touch joysticks while watching live video — no app install, no companion computer, no radio transmitter hardware required.

---

## 2. User-Provided Requirements

These requirements were stated directly by the user at the start of the project.

### 2.1 Hardware target
- **Board**: AI-Thinker ESP32-CAM with OV2640 camera
- **Camera sensor**: OV2640
- **Framework**: Arduino IDE (not ESP-IDF directly)

### 2.2 Connectivity
- WiFi in **Access Point mode** — the ESP32 is the hotspot, no external router
- SSID: `ESPCam`, Password: `ESPCam`, IP: `192.168.0.1`

### 2.3 Control interface
- **Two virtual joysticks** rendered on a mobile browser (touch-based)
- **Mode 2 layout**:
  - Left stick: Throttle (Y axis) / Yaw (X axis)
  - Right stick: Roll (X axis) / Pitch (Y axis)
- Both sticks **snap to center** (1500 µs) on finger release, including throttle
- Arm/disarm: a dedicated button (not a stick switch)
- Flight mode: a **3-position switch** (Stabilize / Acro / Auto) rendered as 3 buttons

### 2.4 RC output
- Protocol: **CRSF** (Crossfire/ELRS), 420000 baud
- Update rate: **100 Hz**
- Transport: **UART2** on the ESP32
- Default pin assignment: TX = GPIO 14 (ESP32 → FC), RX = GPIO 13 (FC → ESP32)
- Library: **AlfredoCRSF** (already available as a sibling project on the machine)

### 2.5 Channel mapping (defaults)
| Channel | Function | Default value |
|---------|----------|---------------|
| CH1 | Roll | 1 |
| CH2 | Pitch | 2 |
| CH3 | Throttle | 3 |
| CH4 | Yaw | 4 |
| CH5 | Arm switch | 5 (low = disarmed, high = armed) |
| CH6 | Flight mode (3-pos) | 6 (low=STB, mid=ACRO, high=AUTO) |

### 2.6 Video
- **MJPEG stream** from OV2640 via browser `<img>` tag
- Default resolution: **QVGA (320×240)** — chosen as balance of frame rate and detail
- Stream must be visible at all times, including in portrait orientation

### 2.7 Configuration
- All runtime parameters configurable through a **web config page** (no reflashing)
- Parameters: TX pin, RX pin, camera resolution, channel numbers for each function
- Settings persisted to **NVS flash** (survive power cycle)
- Config UI accessible via a link on the main controller page

### 2.8 Safety
- **Link timeout**: if no WebSocket message arrives for 500 ms, all channels snap to safe state (sticks to center, arm to disarmed)
- The device must not crash or lock up if not connected to a flight controller (UART TX into the air is harmless)
- Camera failure at boot must not prevent WiFi + CRSF from working

---

## 3. Requirements Discovered During Development

These requirements were not stated upfront but emerged through technical constraints, questions, and iterative testing.

### 3.1 GPIO pin selection for CRSF TX

**Initial choice**: GPIO 12
**Problem discovered**: GPIO 12 is a strapping pin on the ESP32 that controls flash voltage at boot (high = 1.8 V SD mode, low = 3.3 V). Driving it from the FC could corrupt the boot sequence or destroy the flash.
**Iteration**: Moved to GPIO 14 → then GPIO 15 → then back to GPIO 14 after reviewing the tradeoffs.
**Final decision**: **GPIO 14** (SD_CLK / MTMS). Not a strapping pin, no voltage risk, unused when SD card is absent.

> **Lesson**: On the AI-Thinker ESP32-CAM, GPIO 12 must never be used as a general-purpose output. GPIO 15 has a minor strapping effect (ROM boot log on U0TXD) but is safe for CRSF. GPIO 14 has no strapping concerns at all and is the best choice.

### 3.2 Header macro conflict between ESP-IDF and AsyncWebServer

**Problem**: Both `esp_http_server.h` (pulled in by the camera library) and `ESPAsyncWebServer.h` define macros named `HTTP_GET`, `HTTP_POST`, `HTTP_DELETE`, etc. with different values. Including both in one translation unit causes compilation errors.
**Solution**: Camera code (`initCamera`, `startStreamServer`, the MJPEG handler) was moved into its own `.cpp` file (`camera_stream.cpp`). The shared header `camera_stream.h` contains only forward declarations and never includes either conflicting header. The linker resolves across TUs at link time without macro conflicts.

### 3.3 Multiple-definition linker errors for inline config functions

**Problem**: `config.h` defines `loadConfig()` and `saveConfig()` as regular functions. It is included by both `ESP32cam.ino` (TU 1) and `camera_stream.cpp` (TU 2), producing duplicate symbol errors at link time.
**Solution**: Both functions marked `inline`. The C++ ODR allows multiple identical inline definitions across TUs; the linker picks one.

### 3.4 WiFi AP startup sequence sensitivity

**Problem**: Using `WiFi.disconnect(true)` followed by `WiFi.mode(WIFI_OFF)` before starting the AP tore down the WiFi driver entirely. The subsequent `softAP()` call silently failed, leaving the device broadcasting the default `ESP_XXXXXX` SSID instead of `ESPCam`.
**Solution**: Simplified the startup sequence to: `WiFi.mode(WIFI_AP)` → 200 ms delay → `softAP()` → 200 ms delay → `softAPConfig()` → 100 ms delay. The delays allow the driver to settle between state transitions.

### 3.5 Crash on boot when CRSF task starts before UART is initialised

**Problem**: The FreeRTOS CRSF task called `crsf.update()` immediately on start. Because `initCrsf()` had been accidentally omitted from `setup()`, the internal serial port pointer (`_port`) was `NULL`. The first `update()` call dereferenced it → panic → reset loop (reset reason 4 = `ESP_RST_PANIC`).
**Solution**: Added the missing `initCrsf()` call in `setup()` before `xTaskCreatePinnedToCore`. The UART must be initialised before the task is created.

### 3.6 AlfredoCRSF: writePacket vs queuePacket

**Problem**: `AlfredoCRSF::queuePacket()` checks an internal `_linkIsUp` flag before transmitting. This flag is only set when the library detects an incoming CRSF link from a transmitter module. Since the ESP32 is the *source* of RC channels (not a forwarder), `_linkIsUp` never becomes true and `queuePacket()` silently drops every frame.
**Solution**: Use `crsf.writePacket(CRSF_ADDRESS_FLIGHT_CONTROLLER, CRSF_FRAMETYPE_RC_CHANNELS_PACKED, &channels, sizeof(channels))` directly. This bypasses the link-up check and always transmits.

### 3.7 Responsive layout for portrait mode

**Requirement clarified late**: the original UI was landscape-only (side-by-side sticks with video in the center column). When the phone is held in portrait the sticks overlapped or disappeared.
**Solution**: Replaced the flex layout with a CSS Grid layout that uses an `@media (orientation: portrait)` query to switch grid areas:
- Landscape: `[38vw stick] [video / controls] [38vw stick]`
- Portrait: `[video — full width]` / `[stick] [stick]` / `[controls row — full width]`
The joystick canvas `resize()` function already listened to `orientationchange`, so it adapts stick sizes automatically.

### 3.8 Config page link visibility

**Problem**: The config link (`⚙ Config`) on the main page used color `#333` on a `#0d0d0d` background — effectively invisible.
**Solution**: Changed to `color: #4af` with a `border: 1px solid #2a4a6a` — matches the blue accent color used elsewhere in the UI.

---

## 4. Architecture Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Web server | ESPAsyncWebServer + AsyncTCP | Non-blocking; handles WebSocket and HTTP concurrently without dedicated task |
| Stream server | ESP-IDF `esp_http_server` on port 81 | Camera library requires it; isolated to avoid macro conflicts with AsyncWebServer |
| CRSF task | FreeRTOS task pinned to core 0 | Deterministic 100 Hz timing isolated from web/camera workload on core 1 |
| Config storage | `Preferences` (NVS) | Built-in ESP32 Arduino library; wear-levelled, survives power cycles |
| HTML delivery | PROGMEM `const char[]` | Avoids consuming limited DRAM for static page content |
| TX/RX split | AsyncWebServer body callback | `ESPAsyncWebServer` delivers POST body through a separate callback; headers-only callback must also be registered (even if empty) |

---

## 5. Out of Scope

- OTA firmware updates
- SD card recording
- Telemetry display (RSSI, battery voltage, GPS) — RX pin wired but not parsed
- Multiple simultaneous clients (single-operator use case)
- Authentication / access control on the web interface
- Support for other RC protocols (SBUS, PPM, MAVLink)
