
#include <cstdio>

#include "obstacle.h"
#include "test_data.h"

int main() {
    printf("Critical distance: %u cm\n\n", CRITICAL_DISTANCE_CM);

    unsigned obstacles = 0;
    bool previous = false;

    for (uint16_t i = 0; i < TEST_COUNT; i++) {
        uint16_t distance = TEST_DATA[i].distance;
        uint16_t strength = TEST_DATA[i].strength;
        bool obstacle = isObstacle(distance, strength);
        if (obstacle) obstacles++;

        // Only print when the result changes.
        if (i == 0 || obstacle != previous) {
            printf("#%-4u %4u cm  %-9s -> %s\n", i, distance,
                   readingName(filterReading(distance, strength)),
                   obstacle ? "OBSTACLE" : "clear");
        }
        previous = obstacle;
    }

    printf("\n%u readings: %u obstacle, %u clear\n", TEST_COUNT, obstacles, TEST_COUNT - obstacles);
    return 0;
}
