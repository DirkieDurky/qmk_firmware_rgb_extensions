#include "helpers/solid_reactive_simple_custom_base.h"

static bool solid_reactive_simple_custom(effect_params_t *params) {
    return solid_reactive_simple_custom_base(0, params);
}

static bool solid_reactive_simple_custom_delay(effect_params_t *params) {
    return solid_reactive_simple_custom_base(1024, params);
}
