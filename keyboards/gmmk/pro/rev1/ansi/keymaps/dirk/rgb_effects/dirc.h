#include "rgb_effects/helpers/keycodes.h"
#include "rgb_effects/helpers/rgb_print.h"

static bool dirk(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    rgb_print(led_min, led_max, (int[]){K_D, K_I, K_R, K_K}, 4, 8192, 1024, 16, 0);
    return rgb_matrix_check_finished_leds(led_max);
}
