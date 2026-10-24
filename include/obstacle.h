#pragma once
#include <stdint.h>

#include "filter.h"

// Obstacle rule: anything closer than walking speed x reaction time is an obstacle.
const float WALKING_SPEED_CM_S = 100;  // assumed: typical walking pace (1 m/s)
const float REACTION_TIME_S = 1.5;     // assumed: time to notice and react
const uint16_t CRITICAL_DISTANCE_CM = WALKING_SPEED_CM_S * REACTION_TIME_S;  // 150 cm

bool isObstacle(uint16_t distance, uint16_t strength) {
    Reading r = filterReading(distance, strength);
    if (r == TOO_CLOSE) return true;
    return r == VALID && distance < CRITICAL_DISTANCE_CM;
}
