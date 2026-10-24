
#include <cstdio>

#include "obstacle.h"
#include "test_data.h"

int main() {
    printf("Close threshold: %u cm\n\n", CLOSE_THRESHOLD_CM);

    unsigned close_count = 0;
    unsigned drop_count  = 0;
    Detection previous   = CLEAR;

    for (uint16_t i = 0; i < TEST_COUNT; i++) {
        uint16_t  distance  = TEST_DATA[i].distance;
        uint16_t  strength  = TEST_DATA[i].strength;
        Detection det       = detectObstacle(distance, strength);
        if (det == CLOSE) close_count++;
        if (det == DROP)  drop_count++;

        // Only print when the result changes.
        if (i == 0 || det != previous) {
            printf("#%-4u %4u cm  %-9s -> %s\n", i, distance,
                   readingName(filterReading(distance, strength)),
                   detectionName(det));
        }
        previous = det;
    }

    unsigned obstacles = close_count + drop_count;
    printf("\n%u readings: %u obstacle (%u close, %u drop), %u clear\n",
           TEST_COUNT, obstacles, close_count, drop_count, TEST_COUNT - obstacles);
    return 0;
}
