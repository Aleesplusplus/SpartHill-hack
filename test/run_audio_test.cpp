// Audio test: replays TEST_DATA through the obstacle detector and beeps on hazards.
// CLOSE  → steady 440 Hz beep (low, forward obstacle)
// DROP   → rapid 880 Hz beep (high, floor gap)
//
// Requires: macOS with afplay (built-in). No external dependencies.
// Build & run:
//   g++ -std=c++11 -I../include -o run_audio_test run_audio_test.cpp && ./run_audio_test

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>  // usleep

#include "obstacle.h"
#include "test_data.h"

// ---------------------------------------------------------------------------
// Minimal WAV writer — writes a mono 16-bit PCM sine-wave beep to a file.
// ---------------------------------------------------------------------------
static void writeWav(const char* path, float freq_hz, float duration_s,
                     float duty_cycle, int sample_rate = 44100) {
    int total_samples = (int)(duration_s * sample_rate);
    int on_samples    = (int)(total_samples * duty_cycle);  // beep on portion
    int off_samples   = total_samples - on_samples;         // silence tail

    // WAV header (44 bytes)
    uint32_t data_size   = total_samples * sizeof(int16_t);
    uint32_t chunk_size  = 36 + data_size;
    uint16_t num_ch      = 1;
    uint32_t byte_rate   = sample_rate * sizeof(int16_t);
    uint16_t block_align = sizeof(int16_t);
    uint16_t bits        = 16;

    FILE* f = fopen(path, "wb");
    if (!f) { perror("fopen"); return; }

    // RIFF chunk
    fwrite("RIFF", 1, 4, f);
    fwrite(&chunk_size,  4, 1, f);
    fwrite("WAVE", 1, 4, f);
    // fmt  sub-chunk
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16; fwrite(&fmt_size, 4, 1, f);
    uint16_t audio_fmt = 1; fwrite(&audio_fmt, 2, 1, f);  // PCM
    fwrite(&num_ch,      2, 1, f);
    fwrite(&sample_rate, 4, 1, f);  // cast via pointer — same type
    fwrite(&byte_rate,   4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits,        2, 1, f);
    // data sub-chunk
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);

    // PCM samples — sine wave for on_samples, then silence
    for (int i = 0; i < on_samples; i++) {
        // Fade in/out 5 ms to avoid clicks
        int   fade = (int)(0.005f * sample_rate);
        float env  = 1.0f;
        if (i < fade)               env = (float)i / fade;
        if (i > on_samples - fade)  env = (float)(on_samples - i) / fade;

        float    sample = env * 0.6f * 32767.0f *
                          sinf(2.0f * (float)M_PI * freq_hz * i / sample_rate);
        int16_t  s      = (int16_t)sample;
        fwrite(&s, sizeof(s), 1, f);
    }
    int16_t silence = 0;
    for (int i = 0; i < off_samples; i++) fwrite(&silence, sizeof(silence), 1, f);

    fclose(f);
}

// Play a WAV file via afplay (macOS built-in), non-blocking.
static void playAsync(const char* path) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "afplay %s &", path);
    system(cmd);  // NOLINT — intentional, test-only code
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    // Pre-generate the two beep files once.
    const char* close_wav = "/tmp/beep_close.wav";
    const char* drop_wav  = "/tmp/beep_drop.wav";

    // CLOSE: 440 Hz, 0.3 s total, 80% on  — low steady tone
    writeWav(close_wav, 440.0f,  0.3f, 0.8f);
    // DROP:  880 Hz, 0.2 s total, 50% on  — high rapid pulse
    writeWav(drop_wav,  880.0f,  0.2f, 0.5f);

    printf("Close threshold: %u cm\n", CLOSE_THRESHOLD_CM);
    printf("Replaying %u frames — listen for beeps.\n\n", TEST_COUNT);
    printf("  440 Hz beep = OBSTACLE (close)\n");
    printf("  880 Hz beep = OBSTACLE (drop) ← floor gap\n\n");

    unsigned close_count = 0;
    unsigned drop_count  = 0;
    Detection previous   = CLEAR;

    for (uint16_t i = 0; i < TEST_COUNT; i++) {
        uint16_t  distance = TEST_DATA[i].distance;
        uint16_t  strength = TEST_DATA[i].strength;
        Detection det      = detectObstacle(distance, strength);

        if (det == CLOSE) close_count++;
        if (det == DROP)  drop_count++;

        // Print and beep only on transitions, to avoid flooding.
        if (i == 0 || det != previous) {
            printf("#%-4u %4u cm  %-9s -> %s\n", i, distance,
                   readingName(filterReading(distance, strength)),
                   detectionName(det));

            if (det == CLOSE) playAsync(close_wav);
            if (det == DROP)  playAsync(drop_wav);
        }

        previous = det;

        // Pace playback: 4 ms per frame approximates 250 Hz sensor rate.
        usleep(4000);
    }

    unsigned obstacles = close_count + drop_count;
    printf("\n%u readings: %u obstacle (%u close, %u drop), %u clear\n",
           TEST_COUNT, obstacles, close_count, drop_count, TEST_COUNT - obstacles);
    return 0;
}
