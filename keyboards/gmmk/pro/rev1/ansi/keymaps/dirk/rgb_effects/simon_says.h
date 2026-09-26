#include "helpers/solid_reactive_simple_custom_base.h"

uint32_t simon_says_tick = 0;

// The tick since the last recording phase started
uint32_t recording_start = 0;
// The tick since the last repeating phase started
uint32_t repeating_start = 2147483647;

uint32_t simon_says_delay    = 0;
int      length_of_recording = 0;

static bool simon_says(effect_params_t *params) {
    if (simon_says_delay == 0) {
        int ticks_since_last_key             = g_last_hit_tracker.tick[g_last_hit_tracker.count - 1];
        int ticks_since_first_recording_tick = simon_says_tick - recording_start;

        uint16_t max_tick = ticks_since_first_recording_tick;
        uint16_t tick     = max_tick;
        // Find the amount of ticks since the first key of this recording
        for (int8_t j = g_last_hit_tracker.count - 1; j >= 0; j--) {
            if (g_last_hit_tracker.tick[j] < tick) {
                tick = g_last_hit_tracker.tick[j];
                break;
            }
        }
        uint16_t firstKeyTick = tick;

        // If user hasn't pressed a key in a while we start repeating what they did
        // "A while" here is long enough for the lights of all keys being off
        if (ticks_since_last_key > 65535 / rgb_matrix_config.speed && ticks_since_last_key < ticks_since_first_recording_tick) {
            simon_says_delay = firstKeyTick;
            repeating_start  = simon_says_tick;
            // length_of_recording = repeating_start - firstKeyTick;
            // length_of_recording = 1024;
            length_of_recording = simon_says_tick - firstKeyTick;
        }
    } else {
        // Stop repeating when we're done
        if (simon_says_tick >= repeating_start + length_of_recording) {
            simon_says_delay = 0;
            recording_start  = simon_says_tick;
        }
    }

    simon_says_tick += 2;
    return solid_reactive_simple_custom_base(simon_says_delay, params);
}
