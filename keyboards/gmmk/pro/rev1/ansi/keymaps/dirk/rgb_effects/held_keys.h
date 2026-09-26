#pragma once

#include "matrix.h"
#include "helpers/map_colors.h"

// A switch can drive more than one LED: rgb_matrix_map_row_column_to_led_kb()
// lets a keyboard override the mapping, and the LED that g_led_config.matrix_co
// has for the switch is then appended to whatever that hook returned
// (quantum/rgb_matrix/rgb_matrix.c). This keyboard has no such override, so a
// switch always maps to exactly one LED and led_count is 1. The buffer is sized
// for the general case anyway, because the alternative is a stack smash.
#define HELD_KEYS_LED_BUFFER 8
STATIC_ASSERT(HELD_KEYS_LED_BUFFER >= 2, "must hold both the kb hook's result and the matrix_co LED");

// Why the matrix is read directly instead of using g_last_hit_tracker, which
// every other effect in this directory leans on: that tracker can only ever
// report presses. It appends an entry per hit, ages each one through a 16 bit
// millisecond counter, and drops it on overflow (rgb_task_timers), and it has
// no release event at all. So for a key that is still held down it is
// indistinguishable from one released a moment ago - which is the whole
// distinction this effect exists to draw. matrix_get_row() is a plain read of
// the buffer the matrix scan has already filled, so it is both authoritative
// and cheap; simon_says.h reads it the same way for the same reason.
//
// Derived once per frame by held_keys_scan() and then read by the render passes,
// for the same reason simon_says.h keeps simon_says_age[]: the effect is invoked
// once per RGB_MATRIX_LED_PROCESS_LIMIT chunk (~20 LEDs, so 5 times a frame at
// this keyboard's 98), and each chunk only renders its own slice.
static bool held_keys_state[RGB_MATRIX_LED_COUNT];

// Refresh held_keys_state from the matrix. Sampling the level rather than
// watching for edges is what makes this correct when the effect is switched on
// while keys are already down: whatever the matrix says is right immediately,
// with no "was this a fresh press" bookkeeping to seed or a params->init case
// to special case.
static void held_keys_scan(void) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        matrix_row_t state = matrix_get_row(row);

        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            bool down = (state & ((matrix_row_t)1 << col)) != 0;

            // Proxies and unlit positions map to nothing, and then there is
            // simply nothing to switch on or off for this switch.
            uint8_t led[HELD_KEYS_LED_BUFFER];
            uint8_t led_count = rgb_matrix_map_row_column_to_led(row, col, led);
            for (uint8_t i = 0; i < led_count; i++) {
                held_keys_state[led[i]] = down;
            }
        }
    }
}

// Light every key that is physically down, in the colour the RGB_MATRIX_HUE /
// SATURATION / VALUE sliders are set to, and leave everything else dark.
static bool held_keys(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    // Only the first chunk of a frame samples the matrix. Sampling on every
    // chunk would let a key that goes down or up part way through the frame be
    // drawn in some chunks but not others.
    if (params->iter == 0) {
        held_keys_scan();
    }

    for (uint8_t i = led_min; i < led_max; i++) {
        if (held_keys_state[i]) {
            RGB rgb = hsv_to_rgb(map_colors(i));
            rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
        } else {
            rgb_matrix_set_color(i, 0x00, 0x00, 0x00);
        }
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// A fade-out variant would need the same matrix read plus a per-LED record of
// when the key went down, since a release is the only event that starts the
// fade and the level alone cannot tell a fresh release from a long hold.
