#pragma once
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "esp_wifi.h"
#include "config.h"
#include "camera_stream.h"
#include "ui_main.h"
#include "ui_config.h"

// Control state — defined in ESP32cam.ino
extern volatile int           gThrottle;
extern volatile int           gYaw;
extern volatile int           gRoll;
extern volatile int           gPitch;
extern volatile bool          gArmed;
extern volatile int           gFMode;
extern volatile unsigned long gLastWsMsg;
extern volatile bool          gRebootPending;

static AsyncWebServer webServer(80);
static AsyncWebSocket wsServer("/ws");

// ── WebSocket event handler ───────────────────────────────────────────────────
static void onWsEvent(AsyncWebSocket *srv,
                      AsyncWebSocketClient *client,
                      AwsEventType type,
                      void *arg,
                      uint8_t *data,
                      size_t len)
{
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WS connect  id=%u  ip=%s\n",
                      client->id(),
                      client->remoteIP().toString().c_str());

    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WS disconnect id=%u\n", client->id());

    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;

        // Only handle complete, single-frame text messages
        if (info->final && info->index == 0 &&
            info->len == len && info->opcode == WS_TEXT)
        {
            StaticJsonDocument<128> doc;
            DeserializationError err = deserializeJson(doc, (const char *)data, len);
            if (err == DeserializationError::Ok) {
                gThrottle   = doc["t"] | 0;
                gYaw        = doc["y"] | 0;
                gRoll       = doc["r"] | 0;
                gPitch      = doc["p"] | 0;
                gArmed      = (doc["a"] | 0) != 0;
                gFMode      = doc["f"] | 0;
                gLastWsMsg  = millis();
            }
        }

    } else if (type == WS_EVT_ERROR) {
        Serial.printf("WS error id=%u  code=%u\n",
                      client->id(), *((uint16_t *)arg));
    }
}

// ── HTTP handlers ─────────────────────────────────────────────────────────────
static void handleRoot(AsyncWebServerRequest *req) {
    req->send_P(200, "text/html", MAIN_PAGE_HTML);
}

static void handleConfigPage(AsyncWebServerRequest *req) {
    req->send_P(200, "text/html", CONFIG_PAGE_HTML);
}

static void handleConfigData(AsyncWebServerRequest *req) {
    char buf[224];
    snprintf(buf, sizeof(buf),
             "{\"tx_pin\":%d,\"rx_pin\":%d,"
             "\"ch_roll\":%d,\"ch_pitch\":%d,\"ch_throttle\":%d,"
             "\"ch_yaw\":%d,\"ch_arm\":%d,\"ch_fmode\":%d,"
             "\"resolution\":%d}",
             appConfig.tx_pin,      appConfig.rx_pin,
             appConfig.ch_roll,     appConfig.ch_pitch,
             appConfig.ch_throttle, appConfig.ch_yaw,
             appConfig.ch_arm,      appConfig.ch_fmode,
             appConfig.resolution);
    req->send(200, "application/json", buf);
}

// POST /config/save — body is JSON
static void handleConfigBody(AsyncWebServerRequest *req,
                             uint8_t *data, size_t len,
                             size_t /*index*/, size_t /*total*/)
{
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, (const char *)data, len) != DeserializationError::Ok) {
        req->send(400, "text/plain", "Bad JSON");
        return;
    }

    // Only update fields present in the request; keep current value as fallback
    appConfig.tx_pin      = doc["tx_pin"]      | (int)appConfig.tx_pin;
    appConfig.rx_pin      = doc["rx_pin"]      | (int)appConfig.rx_pin;
    appConfig.ch_roll     = doc["ch_roll"]     | (int)appConfig.ch_roll;
    appConfig.ch_pitch    = doc["ch_pitch"]    | (int)appConfig.ch_pitch;
    appConfig.ch_throttle = doc["ch_throttle"] | (int)appConfig.ch_throttle;
    appConfig.ch_yaw      = doc["ch_yaw"]      | (int)appConfig.ch_yaw;
    appConfig.ch_arm      = doc["ch_arm"]      | (int)appConfig.ch_arm;
    appConfig.ch_fmode    = doc["ch_fmode"]    | (int)appConfig.ch_fmode;
    appConfig.resolution  = doc["resolution"]  | (int)appConfig.resolution;

    saveConfig();
    req->send(200, "text/plain", "OK");

    // Schedule reboot outside the callback to avoid stack issues
    gRebootPending = true;
}

// Returns RSSI (dBm) of the first associated WiFi station, or 0 if none connected.
static int8_t readApRssi() {
    wifi_sta_list_t staList;
    if (esp_wifi_ap_get_sta_list(&staList) != ESP_OK || staList.num == 0) return 0;
    return staList.sta[0].rssi;
}

// ── Init ─────────────────────────────────────────────────────────────────────
void initWebServer() {
    wsServer.onEvent(onWsEvent);
    webServer.addHandler(&wsServer);

    webServer.on("/",            HTTP_GET,  handleRoot);
    // Register specific sub-routes before /config — AsyncWebServer uses
    // startsWith matching, so /config would swallow /config/data and /config/save
    webServer.on("/config/data", HTTP_GET,  handleConfigData);
    webServer.on("/config/save", HTTP_POST,
        [](AsyncWebServerRequest *req) {},   // headers-only callback (unused)
        NULL,                                // upload callback (unused)
        handleConfigBody);                   // body callback

    webServer.on("/config",      HTTP_GET,  handleConfigPage);

    webServer.onNotFound([](AsyncWebServerRequest *req) {
        req->send(404, "text/plain", "Not found");
    });

    webServer.begin();
    Serial.println("HTTP server: http://192.168.0.1/");
}

// Call from loop() — must be on the same core as the web server
void webServerLoop() {
    wsServer.cleanupClients();

    // Broadcast WiFi RSSI + camera FPS to all WS clients once per second
    static unsigned long lastRssiMs    = 0;
    static uint32_t      lastFrameCount = 0;
    if (millis() - lastRssiMs >= 1000) {
        lastRssiMs = millis();
        int8_t   rssi = readApRssi();
        uint32_t fps  = gFrameCount - lastFrameCount;
        lastFrameCount = gFrameCount;
        char buf[40];
        snprintf(buf, sizeof(buf), "{\"rssi\":%d,\"fps\":%u}", (int)rssi, (unsigned)fps);
        wsServer.textAll(buf);
    }

    if (gRebootPending) {
        delay(400);
        ESP.restart();
    }
}
