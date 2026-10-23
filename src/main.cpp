
#include <Arduino.h>

uint8_t frame[9];

void setup() {
    Serial.begin(9600);
    Serial2.begin(115200);

    Serial.println("TF-Luna LiDAR Test Started");
}

void loop() {
    // Look for the two header bytes: 0x59 0x59
    if (Serial2.available() < 9) return;

    if (Serial2.peek() != 0x59) {
        Serial2.read();
        return;
    }

    Serial2.read();

    if (Serial2.peek() != 0x59) {
        return;
    }

    Serial2.read();

    frame[0] = 0x59;
    frame[1] = 0x59;

    for (int i = 2; i < 9; i++) {
        frame[i] = Serial2.read();
    }

    // Checksum validation
    uint8_t checksum = 0;

    for (int i = 0; i < 8; i++) {
        checksum += frame[i];
    }

    if (checksum != frame[8]) {
        return;
    }

    // Decode distance and signal strength
    uint16_t distance =
        frame[2] | (frame[3] << 8);

    uint16_t strength =
        frame[4] | (frame[5] << 8);

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.print(" cm | Strength: ");
    Serial.println(strength);

    delay(100);
}
