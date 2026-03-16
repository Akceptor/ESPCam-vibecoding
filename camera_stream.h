#pragma once
#include <stdint.h>
// Declaration-only header — no esp_camera.h / esp_http_server.h here.
// Those headers define HTTP_GET/POST/etc. which clash with ESPAsyncWebServer.
// The full implementation lives in camera_stream.cpp (separate TU).

extern volatile uint32_t gFrameCount;  // incremented by the MJPEG handler each frame

bool initCamera();
void startStreamServer();
