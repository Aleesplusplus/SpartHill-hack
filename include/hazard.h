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
const float FLOOR_TOLERANCE_CM = 6.0f;  // normal unevenness and cane wobble
const float DIP_MAX_CM = 15.0f;         // 6-15 cm lower: pothole or dip
const float STEP_DOWN_MAX_CM = 35.0f;   // 15-35 cm lower: curb or stair; lower is a ledge
const float STEP_UP_MAX_CM = 22.0f;     // 6-22 cm higher: curb or stair up; taller is an obstacle

// TF-Luna sends 100 frames per second.
const uint8_t SMOOTHING_FRAMES = 3;     // median of the last 3 readings removes single-frame spikes
const uint8_t CALIBRATION_FRAMES = 50;  // 0.5 s of steady floor to learn the baseline
const float CALIBRATION_STEADY = 0.05f; // readings must stay within 5% of each other
const uint8_t NO_SIGNAL_FRAMES = 30;
const float BASELINE_ALPHA = 0.02f;     // how fast the baseline follows gentle slopes

// Something raised this close (horizontally) alerts at once: its distance is measured
// directly because the beam hits its front face.
const uint16_t IMMEDIATE_AHEAD_CM = 120;
const uint8_t IMMEDIATE_FRAMES = 3;     // seen in 3 frames...
const uint8_t IMMEDIATE_WINDOW = 60;    // ...within 600 ms (a sweep may only cross it briefly)

// Everything else must be seen for a while. The cane sweeps side to side, so a
// hazard is only in the beam part of the time: count sightings over one full sweep
// instead of requiring them in a row.
const uint8_t SWEEP_WINDOW_FRAMES = 120;  // 1.2 s, about one left-right-left sweep
const uint8_t CONFIRM_SIGHTINGS = 15;     // seen for 150 ms in total within the sweep
const uint8_t CLEAR_SIGHTINGS = 5;        // cleared once it's barely seen for a whole sweep
const uint32_t HOLD_MS = 700;             // a hazard stays at least this long unless a worse one appears

// Walking toward a wall, the beam climbs up its face, so the rise keeps growing.
// A step up levels off at its height. Only call it a step once the rise holds
// within STEADY_RISE_CM for STEADY_RISE_FRAMES; if it keeps growing it becomes an obstacle.
const uint8_t STEADY_RISE_FRAMES = 20;  // 200 ms
const float STEADY_RISE_CM = 4.0f;

// A small offset seen across the whole sweep for this long means the floor itself
// changed (a ramp, a new level, or a new cane angle), so it becomes the new baseline.
// Ledges and obstacles never auto-clear; recalibrate from the app instead.
const uint32_t REBASE_MS = 3000;
const uint8_t REBASE_SIGHTINGS = 108;   // 90% of the sweep window

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

// Higher is more dangerous; a more dangerous hazard can always replace a lesser one.
inline uint8_t hazardSeverity(Hazard h) {
    switch (h) {
        case DIP: return 1;
        case STEP_UP: return 2;
        case STEP_DOWN: return 3;
        case OBSTACLE: return 4;
        case LEDGE: return 5;
        default: return 0;
    }
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
    HazardDetector() { clearSightings(); }

    void recalibrate() {
        calibrationSum = 0;
        calibrationCount = 0;
        smoothingCount = 0;
        riseCount = 0;
        clearSightings();
        state.hazard = CALIBRATING;
    }

    // No frames arriving at all (loose wire or sensor off).
    const HazardState &noData(uint32_t nowMs) {
        if (state.hazard != CALIBRATING) setHazard(NO_SIGNAL, nowMs);
        return state;
    }

    const HazardState &update(uint16_t rawDistanceCm, uint16_t strength, uint32_t nowMs) {
        bool valid = strength >= MIN_STRENGTH && strength != 65535;
        if (!valid) {
            if (weakFrames < 255) weakFrames++;
            if (weakFrames >= NO_SIGNAL_FRAMES) noData(nowMs);
            return state;
        }
        weakFrames = 0;
        uint16_t distanceCm = smooth(rawDistanceCm);

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

        if (state.hazard == NO_SIGNAL) setHazard(FLOOR, nowMs);

        Hazard raw = distanceCm < MIN_RANGE_CM ? OBSTACLE : classifyDepth(depth);
        if (raw == FLOOR) state.baselineCm += BASELINE_ALPHA * (distanceCm - state.baselineCm);

        // Wait for a rise to level off before calling it a step; a wall keeps rising.
        // Up close there's no time to wait, so an unsteady rise is treated as an obstacle.
        bool raised = raw == STEP_UP || raw == OBSTACLE;
        bool close = distanceCm < MIN_RANGE_CM || (raised && state.aheadCm < IMMEDIATE_AHEAD_CM);
        trackRise(raw == STEP_UP ? depth : NAN);
        bool pending = false;
        if (raw == STEP_UP && !riseIsSteady()) {
            if (close) raw = OBSTACLE;
            else pending = true;
        }

        // Close: alert right away.
        closeMask = (closeMask << 1) | (close ? 1 : 0);
        closeMask &= (1ULL << IMMEDIATE_WINDOW) - 1;
        if (!closeMask) closeHazard = FLOOR;
        if (close && hazardSeverity(raw) > hazardSeverity(closeHazard)) closeHazard = raw;
        if (__builtin_popcountll(closeMask) >= IMMEDIATE_FRAMES &&
            hazardSeverity(closeHazard) > hazardSeverity(state.hazard)) {
            setHazard(closeHazard, nowMs);
        }

        // Farther away: alert once it's been seen enough across the sweep.
        addSighting(pending ? CALIBRATING : raw);
        static const Hazard MOST_SEVERE_FIRST[] = {LEDGE, OBSTACLE, STEP_DOWN, STEP_UP, DIP};
        Hazard confirmed = FLOOR;
        for (Hazard h : MOST_SEVERE_FIRST) {
            if (sightings[h] >= CONFIRM_SIGHTINGS) {
                confirmed = h;
                break;
            }
        }
        bool worse = hazardSeverity(confirmed) > hazardSeverity(state.hazard);
        bool faded = state.hazard != FLOOR && sightings[state.hazard] < CLEAR_SIGHTINGS &&
                     nowMs - hazardSinceMs >= HOLD_MS;
        if (worse || faded) setHazard(confirmed, nowMs);

        bool canRebase = state.hazard == DIP || state.hazard == STEP_DOWN || state.hazard == STEP_UP;
        if (canRebase && nowMs - hazardSinceMs > REBASE_MS && sightings[state.hazard] >= REBASE_SIGHTINGS) {
            state.baselineCm = distanceCm;
            clearSightings();
            setHazard(FLOOR, nowMs);
        }
        return state;
    }

private:
    HazardState state = {CALIBRATING, 0, 0, 0};
    float calibrationSum = 0;
    uint8_t calibrationCount = 0;
    uint64_t closeMask = 0;     // one bit per recent frame that saw something close
    Hazard closeHazard = FLOOR;
    uint8_t weakFrames = 0;
    uint32_t hazardSinceMs = 0;

    uint16_t recent[SMOOTHING_FRAMES] = {};
    uint8_t smoothingIndex = 0, smoothingCount = 0;

    float rises[STEADY_RISE_FRAMES] = {};
    uint8_t riseIndex = 0, riseCount = 0;

    // What each frame of the last sweep showed, and how many frames showed each hazard.
    // CALIBRATING marks a frame that counts toward nothing (a rise still growing).
    Hazard window[SWEEP_WINDOW_FRAMES];
    uint8_t windowIndex = 0;
    uint8_t sightings[NO_SIGNAL + 1] = {};

    void clearSightings() {
        for (uint8_t i = 0; i < SWEEP_WINDOW_FRAMES; i++) window[i] = FLOOR;
        for (uint8_t &count : sightings) count = 0;
        sightings[FLOOR] = SWEEP_WINDOW_FRAMES;
    }

    void addSighting(Hazard h) {
        sightings[window[windowIndex]]--;
        window[windowIndex] = h;
        sightings[h]++;
        windowIndex = (windowIndex + 1) % SWEEP_WINDOW_FRAMES;
    }

    void setHazard(Hazard h, uint32_t nowMs) {
        if (h == state.hazard) return;
        state.hazard = h;
        hazardSinceMs = nowMs;
    }

    uint16_t smooth(uint16_t distanceCm) {
        recent[smoothingIndex] = distanceCm;
        smoothingIndex = (smoothingIndex + 1) % SMOOTHING_FRAMES;
        if (smoothingCount < SMOOTHING_FRAMES) smoothingCount++;

        uint16_t sorted[SMOOTHING_FRAMES];
        for (uint8_t i = 0; i < smoothingCount; i++) {
            uint16_t v = recent[i];
            uint8_t j = i;
            for (; j > 0 && sorted[j - 1] > v; j--) sorted[j] = sorted[j - 1];
            sorted[j] = v;
        }
        return sorted[smoothingCount / 2];
    }

    // Pass NAN when the ground isn't raised, which restarts the steadiness check.
    void trackRise(float depth) {
        if (isnan(depth)) {
            riseCount = 0;
            return;
        }
        rises[riseIndex] = depth;
        riseIndex = (riseIndex + 1) % STEADY_RISE_FRAMES;
        if (riseCount < STEADY_RISE_FRAMES) riseCount++;
    }

    bool riseIsSteady() const {
        if (riseCount < STEADY_RISE_FRAMES) return false;
        float lo = rises[0], hi = rises[0];
        for (uint8_t i = 1; i < STEADY_RISE_FRAMES; i++) {
            lo = fminf(lo, rises[i]);
            hi = fmaxf(hi, rises[i]);
        }
        return hi - lo <= STEADY_RISE_CM;
    }
};
