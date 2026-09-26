#include "helpers/keycodes.h"
#include "helpers/map_colors.h"
#include "helpers/held_keys.h"

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

// A group is lit while *any* of its keys is held, and starts fading only once the
// last one is let go. Answering that as a single question per LED, rather than
// letting each key fade on its own release, is what keeps the group together:
// every member asks this on the same frame and gets the same answer, so they all
// turn on together and all start fading on the same frame. It costs a few extra
// calls per frame and no state of its own.
//
// Only the keys under the keycaps decide now, so a group is lit exactly while it
// is being held - before, it stayed lit for the whole fade after any member was
// tapped. The colour is unchanged, and the fade is the shared one, so a group
// that is tapped decays at the same rate as a single key.
static bool tetris_groups_down(uint8_t led) {
    int groupID = findLedGroupId(led);
    if (groupID == -1) return held_keys_switch_down(led);

    for (int8_t k = 0; k < groupSize; k++) {
        if (held_keys_switch_down((uint8_t)groupedActivations[groupID].group[k])) return true;
    }
    return false;
}

static bool tetris_groups(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    if (params->iter == 0) {
        held_keys_step(params->init, tetris_groups_down);
    }

    for (uint8_t i = led_min; i < led_max; i++) {
        int groupID = findLedGroupId(i);
        HSV color   = groupID == -1 ? map_colors(i) : groupedActivations[groupID].color;

        held_keys_fade_paint(i, color);
    }
    return rgb_matrix_check_finished_leds(led_max);
}
