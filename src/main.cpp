#include <Arduino.h>

#include "obstacle.h"

// TF-Luna wiring: sensor TX -> Mega RX2 (pin 17), sensor RX -> Mega TX2 (pin 16), 5V, GND.

// Reads one 9-byte frame from the sensor: 0x59 0x59 DistL DistH StrL StrH TempL TempH Checksum.
// Returns true when a complete, valid frame has arrived.
bool readLuna(uint16_t& distance, uint16_t& strength) {
    static uint8_t buf[9] = {0x59, 0x59};
    static uint8_t idx = 0;

    while (Serial2.available()) {
        uint8_t b = Serial2.read();

        if (idx < 2) {  // waiting for the two 0x59 header bytes
            idx = (b == 0x59) ? idx + 1 : 0;
            continue;
        }

        buf[idx++] = b;
        if (idx < 9) continue;
        idx = 0;

        uint8_t sum = 0;
        for (uint8_t i = 0; i < 8; i++) sum += buf[i];
        if (sum != buf[8]) continue;  // corrupted frame, skip it

        distance = buf[2] | (buf[3] << 8);
        strength = buf[4] | (buf[5] << 8);
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(9600);     // to the computer
    Serial2.begin(115200);  // to the TF-Luna
    Serial.println("TF-Luna obstacle detection started");
}

void loop() {
    uint16_t distance, strength;
    if (!readLuna(distance, strength)) return;

    Reading   result = filterReading(distance, strength);
    Detection det    = detectObstacle(distance, strength);

    char line[64];
    snprintf(line, sizeof(line), "%8lu ms   %4u cm   %6u   %-9s   %s",
             millis(), distance, strength, readingName(result),
             detectionName(det));
    Serial.println(line);
}
