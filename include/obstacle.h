#pragma once
#include <stdint.h>

#include "filter.h"

// Stopping distance: d = v·t + v²/(2a)
// v = 100 cm/s  — worst-case walking speed (Saez et al. 2016)
// t = 0.9 s     — trained-user reaction to haptic/audio cue (Guerrero et al. 2022)
// a = 55 cm/s²  — comfortable deceleration for a cane user (Guerrero et al. 2022)
// Result: 90 + 90.9 ≈ 181 cm
const float    WALKING_SPEED_CM_S  = 100.0f;
const float    REACTION_TIME_S     = 0.9f;
const float    DECELERATION_CM_S2  = 55.0f;
const uint16_t CLOSE_THRESHOLD_CM  =
    (uint16_t)(WALKING_SPEED_CM_S * REACTION_TIME_S
               + (WALKING_SPEED_CM_S * WALKING_SPEED_CM_S) / (2.0f * DECELERATION_CM_S2));

// Drop/gap detector: floor disappearing reads as a sudden positive jump in distance.
// If DROP_CONFIRM of the last DROP_WINDOW valid frames are more than DROP_JUMP_CM
// above the window's floor baseline, the beam has lost the floor — flag as hazard.
const uint16_t DROP_JUMP_CM  = 40;  // minimum gap opening to qualify as a drop
const uint8_t  DROP_WINDOW   = 5;   // number of recent frames to examine
const uint8_t  DROP_CONFIRM  = 3;   // how many of those must show the gap

enum Detection { CLEAR, CLOSE, DROP };

const char* detectionName(Detection d) {
    switch (d) {
        case CLEAR: return "clear";
        case CLOSE: return "OBSTACLE (close)";
        case DROP:  return "OBSTACLE (drop)";
    }
    return "?";
}

Detection detectObstacle(uint16_t distance, uint16_t strength) {
    // Ring buffer declared first so the SATURATED reset can reach it.
    static uint16_t buf[DROP_WINDOW] = {};
    static uint8_t  head             = 0;
    static bool     full             = false;

    Reading r = filterReading(distance, strength);

    // SATURATED: sensor blinded — prior baseline untrustworthy, reset history.
    if (r == SATURATED) { head = 0; full = false; return CLEAR; }

    // TOO_CLOSE / WEAK: gate the frame — buffer no-op, history preserved.
    if (r == TOO_CLOSE) return CLOSE;
    if (r == WEAK)      return CLEAR;

    // Close-approach detector: something is within stopping distance.
    if (distance < CLOSE_THRESHOLD_CM) return CLOSE;

    // Drop/gap detector: push this valid reading into the ring buffer.
    buf[head] = distance;
    head      = (head + 1) % DROP_WINDOW;
    if (head == 0) full = true;

    if (!full) return CLEAR;  // not enough history yet

    // Baseline = minimum distance in the window (the nearest floor reference).
    uint16_t baseline = buf[0];
    for (uint8_t i = 1; i < DROP_WINDOW; i++)
        if (buf[i] < baseline) baseline = buf[i];

    // Count frames where distance jumped well above that baseline.
    uint8_t elevated = 0;
    for (uint8_t i = 0; i < DROP_WINDOW; i++)
        if (buf[i] > baseline + DROP_JUMP_CM) elevated++;

    return (elevated >= DROP_CONFIRM) ? DROP : CLEAR;
}
