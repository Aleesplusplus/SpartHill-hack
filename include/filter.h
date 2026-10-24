#pragma once
#include <stdint.h>

const uint16_t MIN_STRENGTH = 100;          // weaker = unreliable
const uint16_t SATURATED_STRENGTH = 65535;  // sensor overloaded (e.g. sunlight)
const uint16_t MIN_DISTANCE_CM = 20;        // closer = dead zone

enum Reading { VALID, WEAK, SATURATED, TOO_CLOSE };

Reading filterReading(uint16_t distance, uint16_t strength) {
    if (strength == SATURATED_STRENGTH) return SATURATED;
    if (strength < MIN_STRENGTH) return WEAK;
    if (distance < MIN_DISTANCE_CM) return TOO_CLOSE;
    return VALID;
}

const char* readingName(Reading r) {
    switch (r) {
        case VALID:     return "valid";
        case WEAK:      return "WEAK";
        case SATURATED: return "SATURATED";
        case TOO_CLOSE: return "TOO CLOSE";
    }
    return "?";
}
