#pragma once
#include <Arduino.h>

// TF-Luna TX is wired to the pin labelled TX2 (GPIO17), so receive on it.
const int LIDAR_RX_PIN = 17;
const uint32_t LIDAR_BAUD = 115200;

struct LidarReading {
    uint16_t distanceCm;
    uint16_t strength;
    float temperatureC;
};

inline void beginLidar() {
    Serial2.begin(LIDAR_BAUD, SERIAL_8N1, LIDAR_RX_PIN, -1);
}

// Frame: 0x59 0x59 distL distH strL strH tempL tempH checksum
inline bool readLidar(LidarReading &out) {
    static uint8_t frame[9];
    static int index = 0;

    while (Serial2.available()) {
        uint8_t b = Serial2.read();

        if (index < 2 && b != 0x59) {
            index = 0;
            continue;
        }
        frame[index++] = b;
        if (index < 9) continue;
        index = 0;

        uint8_t sum = 0;
        for (int i = 0; i < 8; i++) sum += frame[i];
        if (sum != frame[8]) continue;

        out.distanceCm = frame[2] | (frame[3] << 8);
        out.strength = frame[4] | (frame[5] << 8);
        out.temperatureC = (frame[6] | (frame[7] << 8)) / 8.0f - 256.0f;
        return true;
    }
    return false;
}
