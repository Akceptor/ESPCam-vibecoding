/*
 * ESP32-CAM FPV Drone Controller
 * Board : AI-Thinker ESP32-CAM (OV2640)
 * WiFi  : AP  SSID=ESPCam  PASS=ESPCam  IP=192.168.0.1
 * HTTP  : :80 control page + WS /ws
 * Stream: :81 /stream  (MJPEG)
 * CRSF  : UART2 100 Hz  TX=GPIO14→FC  RX=GPIO13←FC
 */

#include <Arduino.h>
#include <WiFi.h>

// esp_log_level_set is already declared via Arduino.h → esp32-hal-log.h → esp_log.h

#include "config.h"
#include "camera_stream.h"
#include "crsf_output.h"
#include "web_server.h"

// ── WiFi AP ───────────────────────────────────────────────────────────────────
static const char     *AP_SSID   = "ESPCam";
static const char     *AP_PASS   = "ESPCam";
static const IPAddress AP_IP    (192, 168, 0, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

// ── Global control state ──────────────────────────────────────────────────────
volatile int           gThrottle      = 0;
volatile int           gYaw           = 0;
volatile int           gRoll          = 0;
volatile int           gPitch         = 0;
volatile bool          gArmed         = false;
volatile int           gFMode         = 0;
volatile unsigned long gLastWsMsg     = 0;
volatile bool          gRebootPending = false;

Config appConfig = { 14, 13, 1, 2, 3, 4, 5, 6, 5 }; // tx=GPIO14, rx=GPIO13, last=resolution(5=QVGA)

// ── Helpers ───────────────────────────────────────────────────────────────────
static void printBanner() {
    Serial.println();
    Serial.println("╔══════════════════════════════════╗");
    Serial.println("║   ESP32-CAM Drone Controller     ║");
    Serial.println("╚══════════════════════════════════╝");
    Serial.printf ("  Chip: %s  rev%d  cores=%d\n",
                   ESP.getChipModel(),
                   ESP.getChipRevision(),
                   ESP.getChipCores());
    Serial.printf ("  Flash: %u MB\n", ESP.getFlashChipSize() / (1024*1024));
    Serial.printf ("  Free heap: %u B\n", ESP.getFreeHeap());
    Serial.printf ("  PSRAM: %s  (%u B free)\n",
                   psramFound() ? "found" : "NOT found",
                   ESP.getFreePsram());
    Serial.printf ("  Reset reason: %d\n", (int)esp_reset_reason());
    Serial.println();
}

static bool startAP() {
    Serial.println("[WiFi] Starting Access Point...");
    WiFi.mode(WIFI_AP);
    delay(200);

    WiFi.softAP(AP_SSID, AP_PASS);
    delay(200);  // AP needs a moment before softAPConfig

    WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
    delay(100);

    IPAddress ip = WiFi.softAPIP();
    if (ip == IPAddress(0, 0, 0, 0)) {
        Serial.println("[WiFi] ERROR: AP has no IP — check board/antenna");
        return false;
    }

    Serial.printf("[WiFi] AP ready  SSID=%-8s  IP=%s\n", AP_SSID, ip.toString().c_str());
    return true;
}

// ── setup() ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(800);   // wait for monitor to connect and ROM noise to pass
    Serial.print("\n\n\n"); // push past any bootloader garbage

    // Quiet the ESP-IDF log noise on Serial
    esp_log_level_set("*",       ESP_LOG_WARN);
    esp_log_level_set("camera",  ESP_LOG_ERROR);
    esp_log_level_set("httpd",   ESP_LOG_ERROR);

    printBanner();

    // 1. Config
    Serial.println("[CFG]  Loading NVS config...");
    loadConfig();
    Serial.printf("[CFG]  TX=%d  RX=%d  "
                  "Roll=CH%d  Pitch=CH%d  Thr=CH%d  "
                  "Yaw=CH%d  Arm=CH%d  FM=CH%d\n",
                  appConfig.tx_pin,      appConfig.rx_pin,
                  appConfig.ch_roll,     appConfig.ch_pitch,
                  appConfig.ch_throttle, appConfig.ch_yaw,
                  appConfig.ch_arm,      appConfig.ch_fmode);

    // 2. WiFi AP — start BEFORE camera so AP is visible even if camera fails
    if (!startAP()) {
        Serial.println("[WiFi] ERROR: AP failed to start!");
        // continue anyway so CRSF still works
    }

    // 3. Camera
    Serial.println("[CAM]  Initialising OV2640...");
    if (!initCamera()) {
        Serial.println("[CAM]  ERROR: camera init failed — stream will be unavailable.");
        Serial.println("[CAM]  Common causes: wrong board selected, no 5V on cam connector.");
        // Do NOT halt — WiFi + CRSF still usable
    } else {
        // 4. MJPEG stream server (only if camera is up)
        Serial.println("[CAM]  Starting stream server on :81...");
        startStreamServer();
    }

    // 5. HTTP + WebSocket
    Serial.println("[WEB]  Starting HTTP server on :80...");
    initWebServer();

    // 6. CRSF UART + task on core 0
    Serial.println("[CRSF] Initialising UART...");
    initCrsf();
    Serial.println("[CRSF] Starting 100 Hz output task on core 0...");
    xTaskCreatePinnedToCore(crsfTask, "crsf", 4096, NULL, 2, NULL, 0);

    Serial.println();
    Serial.println("════════════════════════════════════");
    Serial.printf ("  Connect to WiFi: %-10s\n", AP_SSID);
    Serial.printf ("  Password:        %-10s\n", AP_PASS);
    Serial.printf ("  Open browser:    http://192.168.0.1/\n");
    Serial.printf ("  Free heap:       %u B\n", ESP.getFreeHeap());
    Serial.println("════════════════════════════════════");
    Serial.println();
}

// ── loop() ────────────────────────────────────────────────────────────────────
void loop() {
    webServerLoop();  // WS cleanup + deferred reboot
    delay(50);
}
