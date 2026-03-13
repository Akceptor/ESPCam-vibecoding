#pragma once
// Declaration-only header — no esp_camera.h / esp_http_server.h here.
// Those headers define HTTP_GET/POST/etc. which clash with ESPAsyncWebServer.
// The full implementation lives in camera_stream.cpp (separate TU).

bool initCamera();
void startStreamServer();
