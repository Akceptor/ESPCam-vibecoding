#pragma once
#include <Preferences.h>

// framesize_t values used for resolution (from esp_camera.h):
//   1 = FRAMESIZE_QQVGA  160x120
//   5 = FRAMESIZE_QVGA   320x240  (default)
//   8 = FRAMESIZE_VGA    640x480
struct Config {
    uint8_t tx_pin;       // CRSF TX → FC
    uint8_t rx_pin;       // CRSF RX ← FC telemetry
    uint8_t ch_roll;      // CRSF channel number 1-16
    uint8_t ch_pitch;
    uint8_t ch_throttle;
    uint8_t ch_yaw;
    uint8_t ch_arm;       // low=disarmed, high=armed
    uint8_t ch_fmode;     // 3-position flight mode
    uint8_t resolution;   // framesize_t value
};

// Defined in ESP32cam.ino
extern Config appConfig;

inline void loadConfig() {
    Preferences prefs;
    prefs.begin("espcam", true);
    appConfig.tx_pin      = prefs.getUChar("tx_pin",       14);
    appConfig.rx_pin      = prefs.getUChar("rx_pin",       13);
    appConfig.ch_roll     = prefs.getUChar("ch_roll",       1);
    appConfig.ch_pitch    = prefs.getUChar("ch_pitch",      2);
    appConfig.ch_throttle = prefs.getUChar("ch_throttle",   3);
    appConfig.ch_yaw      = prefs.getUChar("ch_yaw",        4);
    appConfig.ch_arm      = prefs.getUChar("ch_arm",        5);
    appConfig.ch_fmode    = prefs.getUChar("ch_fmode",      6);
    appConfig.resolution  = prefs.getUChar("resolution",    5); // default QVGA
    prefs.end();
}

inline void saveConfig() {
    Preferences prefs;
    prefs.begin("espcam", false);
    prefs.putUChar("tx_pin",       appConfig.tx_pin);
    prefs.putUChar("rx_pin",       appConfig.rx_pin);
    prefs.putUChar("ch_roll",      appConfig.ch_roll);
    prefs.putUChar("ch_pitch",     appConfig.ch_pitch);
    prefs.putUChar("ch_throttle",  appConfig.ch_throttle);
    prefs.putUChar("ch_yaw",       appConfig.ch_yaw);
    prefs.putUChar("ch_arm",       appConfig.ch_arm);
    prefs.putUChar("ch_fmode",     appConfig.ch_fmode);
    prefs.putUChar("resolution",   appConfig.resolution);
    prefs.end();
}
