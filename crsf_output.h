#pragma once
#include <HardwareSerial.h>
#include <AlfredoCRSF.h>
#include "config.h"

// Control state — written by WebSocket handler, read by CRSF task.
// Declared here, defined in ESP32cam.ino.
extern volatile int           gThrottle;   // -100..100
extern volatile int           gYaw;
extern volatile int           gRoll;
extern volatile int           gPitch;
extern volatile bool          gArmed;
extern volatile int           gFMode;      // 0, 1, 2
extern volatile unsigned long gLastWsMsg;  // millis() of last WS message

// Link timeout: if no WS message for this long, snap to safe state
#define LINK_TIMEOUT_MS 500

static HardwareSerial crsfSerial(2);  // UART2
static AlfredoCRSF    crsf;

// Map joystick value (-100..100) to CRSF channel value
static inline uint16_t axisToCrsf(int val) {
    return (uint16_t)map(constrain(val, -100, 100),
                         -100, 100,
                         CRSF_CHANNEL_VALUE_1000,
                         CRSF_CHANNEL_VALUE_2000);
}

// Set a specific CRSF channel (1-based) by number in the packed struct
static void setCh(crsf_channels_t &ch, int num, uint16_t val) {
    switch (num) {
        case  1: ch.ch0  = val; break;
        case  2: ch.ch1  = val; break;
        case  3: ch.ch2  = val; break;
        case  4: ch.ch3  = val; break;
        case  5: ch.ch4  = val; break;
        case  6: ch.ch5  = val; break;
        case  7: ch.ch6  = val; break;
        case  8: ch.ch7  = val; break;
        case  9: ch.ch8  = val; break;
        case 10: ch.ch9  = val; break;
        case 11: ch.ch10 = val; break;
        case 12: ch.ch11 = val; break;
        case 13: ch.ch12 = val; break;
        case 14: ch.ch13 = val; break;
        case 15: ch.ch14 = val; break;
        case 16: ch.ch15 = val; break;
        default: break;
    }
}

static void sendCrsfFrame() {
    crsf_channels_t channels;

    // Default all channels to mid
    channels.ch0  = CRSF_CHANNEL_VALUE_MID;
    channels.ch1  = CRSF_CHANNEL_VALUE_MID;
    channels.ch2  = CRSF_CHANNEL_VALUE_MID;
    channels.ch3  = CRSF_CHANNEL_VALUE_MID;
    channels.ch4  = CRSF_CHANNEL_VALUE_MID;
    channels.ch5  = CRSF_CHANNEL_VALUE_MID;
    channels.ch6  = CRSF_CHANNEL_VALUE_MID;
    channels.ch7  = CRSF_CHANNEL_VALUE_MID;
    channels.ch8  = CRSF_CHANNEL_VALUE_MID;
    channels.ch9  = CRSF_CHANNEL_VALUE_MID;
    channels.ch10 = CRSF_CHANNEL_VALUE_MID;
    channels.ch11 = CRSF_CHANNEL_VALUE_MID;
    channels.ch12 = CRSF_CHANNEL_VALUE_MID;
    channels.ch13 = CRSF_CHANNEL_VALUE_MID;
    channels.ch14 = CRSF_CHANNEL_VALUE_MID;
    channels.ch15 = CRSF_CHANNEL_VALUE_MID;

    bool linkOk = (millis() - gLastWsMsg) < LINK_TIMEOUT_MS;

    uint16_t rVal = linkOk ? axisToCrsf(gRoll)     : CRSF_CHANNEL_VALUE_MID;
    uint16_t pVal = linkOk ? axisToCrsf(gPitch)    : CRSF_CHANNEL_VALUE_MID;
    uint16_t tVal = linkOk ? axisToCrsf(gThrottle) : CRSF_CHANNEL_VALUE_MID;
    uint16_t yVal = linkOk ? axisToCrsf(gYaw)      : CRSF_CHANNEL_VALUE_MID;

    // Arm: high=armed, low=disarmed. Always disarm on link loss.
    uint16_t aVal = (linkOk && gArmed)
                    ? CRSF_CHANNEL_VALUE_2000
                    : CRSF_CHANNEL_VALUE_1000;

    // 3-position flight mode
    static const uint16_t fmVals[3] = {
        CRSF_CHANNEL_VALUE_1000,
        CRSF_CHANNEL_VALUE_MID,
        CRSF_CHANNEL_VALUE_2000
    };
    uint16_t fVal = linkOk ? fmVals[constrain((int)gFMode, 0, 2)] : CRSF_CHANNEL_VALUE_1000;

    setCh(channels, appConfig.ch_roll,     rVal);
    setCh(channels, appConfig.ch_pitch,    pVal);
    setCh(channels, appConfig.ch_throttle, tVal);
    setCh(channels, appConfig.ch_yaw,      yVal);
    setCh(channels, appConfig.ch_arm,      aVal);
    setCh(channels, appConfig.ch_fmode,    fVal);

    // writePacket (not queuePacket) — we are the packet source, not forwarding.
    // queuePacket requires _linkIsUp which is never set since we generate channels.
    crsf.writePacket(CRSF_ADDRESS_FLIGHT_CONTROLLER,
                     CRSF_FRAMETYPE_RC_CHANNELS_PACKED,
                     &channels, sizeof(channels));
}

void initCrsf() {
    // GPIO 14 (SD_CLK / MTMS) — not a strapping pin, no voltage risk.
    // GPIO 13 (SD_DATA3) — no strapping concerns.
    // Neither pin is used by PSRAM or the camera interface.
    crsfSerial.begin(CRSF_BAUDRATE, SERIAL_8N1,
                     appConfig.rx_pin,   // RX (FC → ESP)
                     appConfig.tx_pin);  // TX (ESP → FC)
    crsf.begin(crsfSerial);
    Serial.printf("CRSF UART2: TX=GPIO%d  RX=GPIO%d  %d baud\n",
                  appConfig.tx_pin, appConfig.rx_pin, CRSF_BAUDRATE);
}

// FreeRTOS task — runs at 100 Hz on core 0
void crsfTask(void *param) {
    const TickType_t period = pdMS_TO_TICKS(10);
    TickType_t       lastWake = xTaskGetTickCount();
    while (true) {
        crsf.update();    // process incoming telemetry from FC
        sendCrsfFrame();  // transmit RC channels to FC
        vTaskDelayUntil(&lastWake, period);
    }
}
