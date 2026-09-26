#pragma once

long printTick = 0;

static bool rgb_print(uint8_t led_min, uint8_t led_max, int keyCodes[], int count, int keyLength, int intervalLength, int loopLength, int totalLength) {
    // Simple way to prevent the same output from appearing multiple times
    if (totalLength == 1) totalLength = 2;

    int index = printTick / (keyLength + intervalLength);
    if ((totalLength != 0 && index >= totalLength)) {
        return true;
    }
    if (loopLength != 0) {
        index %= loopLength;
    }

    RGB rgb = rgb_matrix_hsv_to_rgb(rgb_matrix_config.hsv);

    for (uint8_t i = led_min; i < led_max; i++) {
        if (index < count && i == keyCodes[index] && printTick % (keyLength + intervalLength) <= keyLength) {
            rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
        } else {
            rgb_matrix_set_color(i, 0x00, 0x00, 0x00);
        }
    }

    printTick += rgb_matrix_config.speed;
    return false;
}
