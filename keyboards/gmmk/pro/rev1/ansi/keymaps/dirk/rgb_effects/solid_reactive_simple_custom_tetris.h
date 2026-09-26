#include "helpers/keycodes.h"
#include "helpers/map_colors.h"
#include "helpers/reactive_fade.h"

#define groupSize 4
struct GroupedActivation {
    HSV color;
    int group[groupSize];
};

#define groupedActivationCount 5
struct GroupedActivation groupedActivations[groupedActivationCount] = {
    {(HSV){139, 191, 255}, {K_F1, K_F2, K_F3, K_F4}}, {(HSV){139, 191, 255}, {K_F5, K_F6, K_F7, K_F8}}, {(HSV){139, 191, 255}, {K_F9, K_F10, K_F11, K_F12}}, {(HSV){139, 191, 255}, {K_DELETE, K_HOME, K_END, K_F13}}, {(HSV){225, 145, 255}, {K_UP, K_DOWN, K_LEFT, K_RIGHT}},
};

static int findLedGroupId(int id) {
    for (int i = 0; i < groupedActivationCount; i++) {
        for (int j = 0; j < groupSize; j++) {
            if (groupedActivations[i].group[j] == id) {
                return i;
            }
        }
    }
    return -1;
}

static bool solid_reactive_simple_custom_tetris(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    uint16_t window = reactive_fade_window(rgb_matrix_config.speed, REACTIVE_FADE_MIN_MS, REACTIVE_FADE_MAX_MS);
    for (uint8_t i = led_min; i < led_max; i++) {
        HSV      color;
        uint16_t tick;

        int groupID = findLedGroupId(i);
        if (groupID == -1) {
            color = map_colors(i);

            // Reverse search to find most recent key hit
            tick = window;
            for (int8_t j = g_last_hit_tracker.count - 1; j >= 0; j--) {
                if (g_last_hit_tracker.index[j] == i && g_last_hit_tracker.tick[j] < tick) {
                    tick = g_last_hit_tracker.tick[j];
                    break;
                }
            }
        } else {
            color = groupedActivations[groupID].color;

            bool keyInGroupPressed = false;
            // Search if any group members have been pressed
            tick = window;
            for (int8_t k = 0; k < groupSize; k++) {
                for (int8_t l = g_last_hit_tracker.count - 1; l >= 0; l--) {
                    if (g_last_hit_tracker.index[l] == groupedActivations[groupID].group[k] && g_last_hit_tracker.tick[l] < tick) {
                        keyInGroupPressed = true;
                        tick              = g_last_hit_tracker.tick[l];
                    }
                }
            }
            if (!keyInGroupPressed) continue;
        }

        uint16_t offset = reactive_fade_offset(tick, window);
        HSV      hsv    = SOLID_REACTIVE_SIMPLE_math(color, offset);
        RGB      rgb    = hsv_to_rgb(hsv);

        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}
