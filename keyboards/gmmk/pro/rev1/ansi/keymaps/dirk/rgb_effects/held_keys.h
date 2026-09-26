#include "helpers/map_colors.h"

// static bool held_keys(effect_params_t *params) {
//     RGB_MATRIX_USE_LIMITS(led_min, led_max);
//     for (uint8_t i = led_min; i < led_max; i++) {
//         if (g_held_keys_tracker[i]) {
//             // HSV color = map_colors(i);

//             // uint16_t offset = scale16by8(tick, rgb_matrix_config.speed);
//             // HSV      hsv    = SOLID_REACTIVE_SIMPLE_math(color, offset);
//             // RGB      rgb    = hsv_to_rgb(hsv);
//             rgb_matrix_set_color(i, 0xff, 0xff, 0xff);
//         } else {
//             rgb_matrix_set_color(i, 0x00, 0x00, 0x00);
//         }
//     }

//     return rgb_matrix_check_finished_leds(led_max);
// }

// static bool held_keys_fade_out(effect_params_t *params) {
//     RGB_MATRIX_USE_LIMITS(led_min, led_max);

//     uint16_t max_tick = 65535 / rgb_matrix_config.speed;
//     for (uint8_t i = led_min; i < led_max; i++) {
//         // Reverse search to find most recent key-release or press of this key
//         uint16_t last_key_tick    = max_tick;
//         bool     last_key_pressed = false;
//         bool     hit_found        = false;
//         for (int8_t j = g_last_hit_tracker.count - 1; j >= 0; j--) {
//             if (g_last_hit_tracker.index[j] == i && g_last_hit_tracker.tick[j] < (last_key_tick)) {
//                 last_key_tick    = g_last_hit_tracker.tick[j];
//                 last_key_pressed = g_last_hit_tracker.pressed[j];
//                 hit_found        = true;
//             }
//         }

//         uint16_t tick = max_tick;
//         if (hit_found) {
//             if (last_key_pressed) {
//                 tick = 0;
//             } else {
//                 tick = last_key_tick;
//             }
//         }

//         HSV color = map_colors(i);

//         uint16_t offset = scale16by8(tick, rgb_matrix_config.speed);
//         HSV      hsv    = SOLID_REACTIVE_SIMPLE_math(color, offset);
//         RGB      rgb    = hsv_to_rgb(hsv);
//         rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
//     }
//     return rgb_matrix_check_finished_leds(led_max);
// }
