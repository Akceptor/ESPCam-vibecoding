// camera_stream.cpp — compiled as a separate translation unit so that
// esp_camera.h / esp_http_server.h (which define HTTP_GET, HTTP_POST, …)
// never share a TU with ESPAsyncWebServer.h.

#include "camera_stream.h"
#include "config.h"        // appConfig.resolution
#include "esp_camera.h"
#include "esp_http_server.h"
#include <Arduino.h>   // Serial, psramFound, etc.

// ── AI-Thinker ESP32-CAM (OV2640) pin map ────────────────────────────────────
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK     0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

#define STREAM_CONTENT_TYPE "multipart/x-mixed-replace;boundary=fb"
#define STREAM_BOUNDARY     "\r\n--fb\r\n"
#define STREAM_PART         "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n"

static httpd_handle_t streamHttpd = NULL;

volatile uint32_t gFrameCount = 0;  // read by web_server.h for FPS calculation

// ── MJPEG stream handler ──────────────────────────────────────────────────────
static esp_err_t streamHandler(httpd_req_t *req) {
    camera_fb_t *fb     = NULL;
    esp_err_t    res    = ESP_OK;
    uint8_t     *jpgBuf = NULL;
    size_t       jpgLen = 0;
    char         partBuf[64];

    httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60");

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        if (fb->format != PIXFORMAT_JPEG) {
            bool ok = frame2jpg(fb, 80, &jpgBuf, &jpgLen);
            esp_camera_fb_return(fb);
            fb = NULL;
            if (!ok) {
                Serial.println("JPEG conversion failed");
                res = ESP_FAIL;
                break;
            }
        } else {
            jpgLen = fb->len;
            jpgBuf = fb->buf;
        }

        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if (res == ESP_OK) {
            size_t hlen = snprintf(partBuf, sizeof(partBuf), STREAM_PART, jpgLen);
            res = httpd_resp_send_chunk(req, partBuf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)jpgBuf, jpgLen);
        }

        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        } else if (jpgBuf) {
            free(jpgBuf);
            jpgBuf = NULL;
        }

        if (res != ESP_OK) break;
        gFrameCount++;
    }
    return res;
}

// ── Camera initialisation ─────────────────────────────────────────────────────
bool initCamera() {
    camera_config_t cfg;
    cfg.ledc_channel  = LEDC_CHANNEL_0;
    cfg.ledc_timer    = LEDC_TIMER_0;
    cfg.pin_d0        = CAM_PIN_D0;
    cfg.pin_d1        = CAM_PIN_D1;
    cfg.pin_d2        = CAM_PIN_D2;
    cfg.pin_d3        = CAM_PIN_D3;
    cfg.pin_d4        = CAM_PIN_D4;
    cfg.pin_d5        = CAM_PIN_D5;
    cfg.pin_d6        = CAM_PIN_D6;
    cfg.pin_d7        = CAM_PIN_D7;
    cfg.pin_xclk      = CAM_PIN_XCLK;
    cfg.pin_pclk      = CAM_PIN_PCLK;
    cfg.pin_vsync     = CAM_PIN_VSYNC;
    cfg.pin_href      = CAM_PIN_HREF;
    cfg.pin_sscb_sda  = CAM_PIN_SIOD;
    cfg.pin_sscb_scl  = CAM_PIN_SIOC;
    cfg.pin_pwdn      = CAM_PIN_PWDN;
    cfg.pin_reset     = CAM_PIN_RESET;
    cfg.xclk_freq_hz  = 20000000;
    cfg.pixel_format  = PIXFORMAT_JPEG;

    framesize_t fs = (framesize_t)appConfig.resolution;
    if (psramFound()) {
        cfg.frame_size   = fs;
        cfg.jpeg_quality = 12;
        cfg.fb_count     = 2;
        cfg.grab_mode    = CAMERA_GRAB_LATEST;
    } else {
        cfg.frame_size   = fs;
        cfg.jpeg_quality = 15;
        cfg.fb_count     = 1;
        cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
    }

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s,    (framesize_t)appConfig.resolution);
        s->set_quality(s,      12);
        s->set_brightness(s,   1);
        s->set_saturation(s,   0);
        s->set_sharpness(s,    0);
        s->set_whitebal(s,     1);
        s->set_awb_gain(s,     1);
        s->set_exposure_ctrl(s, 1);
        s->set_aec2(s,         1);
    }

    Serial.println("Camera ready (QVGA JPEG)");
    return true;
}

// ── Stream httpd (port 81) ────────────────────────────────────────────────────
void startStreamServer() {
    httpd_config_t cfg   = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = 81;
    cfg.ctrl_port        = 32769;
    cfg.max_uri_handlers = 2;

    httpd_uri_t streamUri = {
        .uri      = "/stream",
        .method   = HTTP_GET,
        .handler  = streamHandler,
        .user_ctx = NULL
    };

    if (httpd_start(&streamHttpd, &cfg) == ESP_OK) {
        httpd_register_uri_handler(streamHttpd, &streamUri);
        Serial.println("Stream: http://192.168.0.1:81/stream");
    } else {
        Serial.println("Stream server failed to start");
    }
}
