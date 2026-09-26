#pragma once

#include "map_colors.h"
#include "reactive_fade.h"

// delay_ms is a real time in milliseconds, not a number of ticks: a key press
// stays invisible for that long, then fades over REACTIVE_FADE_*_MS.
//
// It used to be compared against the fade window, which was 65535 / speed, so
// any delay above that could never match and every LED stayed at the window,
// i.e. fully black. solid_reactive_simple_custom_delay(1024) was therefore dark
// at any speed above about 63. Deriving the window from the slider (now at most
// REACTIVE_FADE_MAX_MS) would have made that true at every speed, so the delay
// is applied to the age of the hit instead of to the window it is compared
// against, which is what the original arithmetic was reaching for.
static bool solid_reactive_simple_custom_base(uint16_t delay_ms, effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    uint16_t window = reactive_fade_window(rgb_matrix_config.speed, REACTIVE_FADE_MIN_MS, REACTIVE_FADE_MAX_MS);

    for (uint8_t i = led_min; i < led_max; i++) {
        // Start fully faded, then keep the most recent hit that is old enough to
        // be shown at all. Searching every entry rather than breaking on the
        // first match is what makes a key pressed twice show its latest press.
        uint32_t age = window;
        for (int8_t j = g_last_hit_tracker.count - 1; j >= 0; j--) {
            if (g_last_hit_tracker.index[j] != i) continue;

            uint32_t hit_age = g_last_hit_tracker.tick[j];
            if (hit_age < delay_ms) continue;  // still inside the delay, not shown yet
            hit_age -= delay_ms;                // age counted from when it became visible

            if (hit_age < age) age = hit_age;
        }

        HSV color = map_colors(i);

        uint16_t offset = reactive_fade_offset(age, window);
        HSV      hsv    = SOLID_REACTIVE_SIMPLE_math(color, offset);
        RGB      rgb    = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}
