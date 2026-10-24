#pragma once
#include <Arduino.h>
#include <math.h>

// The lidar points forward and down, so its normal reading is the floor ahead.
// Everything is judged against that floor baseline, not a fixed distance.
//
// For a beam at angle a below horizontal, from height H:
//   sin(a) = H / baseline
//   vertical change of the ground = (reading - baseline) * sin(a)
//   horizontal distance ahead     = reading * cos(a)

// Measure from the floor to the lidar lens while the cane is held normally.
const float SENSOR_HEIGHT_CM = 75.0f;

const uint16_t MIN_STRENGTH = 100;   // below this the distance is unreliable
const uint16_t MIN_RANGE_CM = 20;    // TF-Luna can't measure closer than this

// Vertical change (cm) relative to the floor that defines each hazard.
const float FLOOR_TOLERANCE_CM = 4.0f;  // normal unevenness and cane wobble
const float DIP_MAX_CM = 15.0f;         // 4-15 cm lower: pothole or dip
const float STEP_DOWN_MAX_CM = 35.0f;   // 15-35 cm lower: curb or stair; lower is a ledge
const float STEP_UP_MAX_CM = 30.0f;     // 4-30 cm higher: curb or stair up; taller is an obstacle

// TF-Luna sends 100 frames per second.
const uint8_t CALIBRATION_FRAMES = 50;  // 0.5 s of steady floor to learn the baseline
const float CALIBRATION_STEADY = 0.05f; // readings must stay within 5% of each other
const uint8_t CONFIRM_FRAMES = 4;       // hazard must persist 40 ms before alerting
const uint8_t CLEAR_FRAMES = 10;        // floor must persist 100 ms before clearing
const uint8_t NO_SIGNAL_FRAMES = 30;
const float BASELINE_ALPHA = 0.02f;     // how fast the baseline follows gentle slopes

// A small offset that lasts this long means the floor itself changed (a ramp,
// a new level, or a new cane angle), so it becomes the new baseline.
// Ledges and obstacles never auto-clear; recalibrate from the app instead.
const uint32_t REBASE_MS = 3000;

enum Hazard : uint8_t { CALIBRATING, FLOOR, DIP, STEP_DOWN, LEDGE, STEP_UP, OBSTACLE, NO_SIGNAL };

inline const char *hazardName(Hazard h) {
    switch (h) {
        case CALIBRATING: return "CALIBRATING";
        case FLOOR: return "FLOOR";
        case DIP: return "DIP";
        case STEP_DOWN: return "STEP_DOWN";
        case LEDGE: return "LEDGE";
        case STEP_UP: return "STEP_UP";
        case OBSTACLE: return "OBSTACLE";
        case NO_SIGNAL: return "NO_SIGNAL";
    }
    return "?";
}

struct HazardState {
    Hazard hazard;
    float baselineCm;
    int16_t depthCm;    // positive = ground is lower than the floor, negative = higher
    uint16_t aheadCm;   // horizontal distance to where the beam lands
};

inline Hazard classifyDepth(float depthCm) {
    if (depthCm > STEP_DOWN_MAX_CM) return LEDGE;
    if (depthCm > DIP_MAX_CM) return STEP_DOWN;
    if (depthCm > FLOOR_TOLERANCE_CM) return DIP;
    if (depthCm < -STEP_UP_MAX_CM) return OBSTACLE;
    if (depthCm < -FLOOR_TOLERANCE_CM) return STEP_UP;
    return FLOOR;
}

class HazardDetector {
public:
    void recalibrate() {
        calibrationSum = 0;
        calibrationCount = 0;
        state.hazard = CALIBRATING;
    }

    // No frames arriving at all (loose wire or sensor off).
    const HazardState &noData(uint32_t nowMs) {
        if (state.hazard != CALIBRATING) setHazard(NO_SIGNAL, nowMs);
        return state;
    }

    const HazardState &update(uint16_t distanceCm, uint16_t strength, uint32_t nowMs) {
        bool valid = strength >= MIN_STRENGTH && strength != 65535;
        if (!valid) {
            if (weakFrames < 255) weakFrames++;
            if (weakFrames >= NO_SIGNAL_FRAMES) noData(nowMs);
            return state;
        }
        weakFrames = 0;

        if (state.hazard == CALIBRATING) {
            if (distanceCm < MIN_RANGE_CM) return state;
            // Only a steady reading is the floor; start over if the cane is still moving.
            if (calibrationCount > 0) {
                float mean = calibrationSum / calibrationCount;
                if (fabsf(distanceCm - mean) > mean * CALIBRATION_STEADY) {
                    calibrationSum = 0;
                    calibrationCount = 0;
                }
            }
            calibrationSum += distanceCm;
            if (++calibrationCount >= CALIBRATION_FRAMES) {
                state.baselineCm = calibrationSum / calibrationCount;
                setHazard(FLOOR, nowMs);
            }
            return state;
        }

        float sinA = fminf(1.0f, SENSOR_HEIGHT_CM / state.baselineCm);
        float cosA = sqrtf(1.0f - sinA * sinA);
        float depth = (distanceCm - state.baselineCm) * sinA;
        state.depthCm = (int16_t)lroundf(depth);
        state.aheadCm = (uint16_t)(distanceCm * cosA);

        Hazard raw = distanceCm < MIN_RANGE_CM ? OBSTACLE : classifyDepth(depth);
        if (raw == FLOOR) state.baselineCm += BASELINE_ALPHA * (distanceCm - state.baselineCm);

        if (raw == candidate) {
            if (candidateFrames < 255) candidateFrames++;
        } else {
            candidate = raw;
            candidateFrames = 1;
        }
        uint8_t needed = candidate == FLOOR ? CLEAR_FRAMES : CONFIRM_FRAMES;
        if (candidate != state.hazard && candidateFrames >= needed) setHazard(candidate, nowMs);

        bool canRebase = state.hazard == DIP || state.hazard == STEP_DOWN || state.hazard == STEP_UP;
        if (canRebase && nowMs - hazardSinceMs > REBASE_MS) {
            state.baselineCm = distanceCm;
            setHazard(FLOOR, nowMs);
        }
        return state;
    }

private:
    HazardState state = {CALIBRATING, 0, 0, 0};
    float calibrationSum = 0;
    uint8_t calibrationCount = 0;
    Hazard candidate = FLOOR;
    uint8_t candidateFrames = 0;
    uint8_t weakFrames = 0;
    uint32_t hazardSinceMs = 0;

    void setHazard(Hazard h, uint32_t nowMs) {
        if (h == state.hazard) return;
        state.hazard = h;
        hazardSinceMs = nowMs;
    }
};
