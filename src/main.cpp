#include <Arduino.h>

#include "ble_link.h"
#include "hazard.h"
#include "lidar.h"
#include "web_dashboard.h"

const uint32_t SEND_INTERVAL_MS = 250;   // keep the phone's live readout fresh
const uint32_t PRINT_INTERVAL_MS = 100;
const uint32_t NO_DATA_MS = 500;

HazardDetector detector;

void setup() {
    Serial.begin(115200);
    beginLidar();
    beginBle();
    beginDashboard();
    Serial.println("SparkCane started - hold the cane normally over flat ground to calibrate");
}

void loop() {
    static uint32_t lastFrame = 0, lastSend = 0, lastPrint = 0;
    static Hazard lastSent = NO_SIGNAL;
    static uint16_t distanceCm = 0;
    uint32_t now = millis();

    handleDashboard();

    if (recalibrateRequested) {
        recalibrateRequested = false;
        detector.recalibrate();
        Serial.println("recalibrating");
    }

    LidarReading reading;
    const HazardState *state = nullptr;
    if (readLidar(reading)) {
        lastFrame = now;
        distanceCm = reading.distanceCm;
        state = &detector.update(reading.distanceCm, reading.strength, now);
    } else if (now - lastFrame > NO_DATA_MS) {
        state = &detector.noData(now);
    }
    if (!state) return;
    updateDashboard(*state, distanceCm);

    if (state->hazard != lastSent || now - lastSend >= SEND_INTERVAL_MS) {
        char message[48];
        snprintf(message, sizeof(message), "%s,%d,%u,%u", hazardName(state->hazard),
                 state->depthCm, state->aheadCm, distanceCm);
        sendHazard(message);
        lastSent = state->hazard;
        lastSend = now;
    }

    if (now - lastPrint >= PRINT_INTERVAL_MS) {
        lastPrint = now;
        Serial.printf("%-11s  dist %4u cm  floor %5.1f cm  depth %+4d cm  ahead %4u cm  %s\n",
                      hazardName(state->hazard), distanceCm, state->baselineCm, state->depthCm,
                      state->aheadCm, phoneConnected ? "phone" : "");
    }
}
