#pragma once

#include "map_colors.h"

static bool solid_reactive_simple_custom_base(int delay, effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    uint16_t max_tick = 65535 / rgb_matrix_config.speed;
    for (uint8_t i = led_min; i < led_max; i++) {
        // Reverse search to find most recent key-release or press of this key
        uint16_t tick = max_tick;
        for (int8_t j = g_last_hit_tracker.count - 1; j >= 0; j--) {
            if (g_last_hit_tracker.index[j] == i && tick >= delay && g_last_hit_tracker.tick[j] < (tick + delay)) {
                tick = g_last_hit_tracker.tick[j] - delay;
            }
        }

        HSV color = map_colors(i);

        uint16_t offset = scale16by8(tick, rgb_matrix_config.speed);
        HSV      hsv    = SOLID_REACTIVE_SIMPLE_math(color, offset);
        RGB      rgb    = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}
