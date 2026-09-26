RGB_MATRIX_EFFECT(dirk)
RGB_MATRIX_EFFECT(solid_reactive_simple_custom)
RGB_MATRIX_EFFECT(solid_reactive_simple_custom_delay)
RGB_MATRIX_EFFECT(solid_reactive_simple_custom2)
RGB_MATRIX_EFFECT(calculator)

// RGB_MATRIX_EFFECT(simon_says)
// RGB_MATRIX_EFFECT(held_keys)
// RGB_MATRIX_EFFECT(held_keys_fade_out)

#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

enum keycodes {
    K_ESC                  = 0,
    K_GRAVE                = 1,
    K_TAB                  = 2,
    K_CAPS_LOCK            = 3,
    K_LEFT_SHIFT           = 4,
    K_LEFT_CONTROL         = 5,
    K_F1                   = 6,
    K_1                    = 7,
    K_Q                    = 8,
    K_A                    = 9,
    K_Z                    = 10,
    K_LEFT_SUPER           = 11,
    K_F2                   = 12,
    K_2                    = 13,
    K_W                    = 14,
    K_S                    = 15,
    K_X                    = 16,
    K_LEFT_ALT             = 17,
    K_F3                   = 18,
    K_3                    = 19,
    K_E                    = 20,
    K_D                    = 21,
    K_C                    = 22,
    K_F4                   = 23,
    K_4                    = 24,
    K_R                    = 25,
    K_F                    = 26,
    K_V                    = 27,
    K_F5                   = 28,
    K_5                    = 29,
    K_T                    = 30,
    K_G                    = 31,
    K_B                    = 32,
    K_SPACE                = 33,
    K_F6                   = 34,
    K_6                    = 35,
    K_Y                    = 36,
    K_H                    = 37,
    K_N                    = 38,
    K_F7                   = 39,
    K_7                    = 40,
    K_U                    = 41,
    K_J                    = 42,
    K_M                    = 43,
    K_F8                   = 44,
    K_8                    = 45,
    K_I                    = 46,
    K_K                    = 47,
    K_COMMA                = 48,
    K_RIGHT_ALT            = 49,
    K_F9                   = 50,
    K_9                    = 51,
    K_O                    = 52,
    K_L                    = 53,
    K_DOT                  = 54,
    K_RIGHT_SUPER          = 55,
    K_F10                  = 56,
    K_0                    = 57,
    K_P                    = 58,
    K_SEMICOLON            = 59,
    K_SLASH                = 60,
    K_F11                  = 61,
    K_MINUS                = 62,
    K_LEFT_SQUARE_BRACKET  = 63,
    K_APOSTROPHE           = 64,
    K_RIGHT_CONTROL        = 65,
    K_F12                  = 66,
    K_SIDE_LEFT_1          = 67,
    K_SIDE_RIGHT_1         = 68,
    K_PRINT_SCREEN         = 69,
    K_SIDE_LEFT_2          = 70,
    K_SIDE_RIGHT_2         = 71,
    K_DELETE               = 72,
    K_SIDE_LEFT_3          = 73,
    K_SIDE_RIGHT_3         = 74,
    K_HOME                 = 75,
    K_SIDE_LEFT_4          = 76,
    K_SIDE_RIGHT_4         = 77,
    K_EQUALS               = 78,
    K_RIGHT                = 79,
    K_SIDE_LEFT_5          = 80,
    K_SIDE_RIGHT_5         = 81,
    K_F13                  = 82,
    K_SIDE_LEFT_6          = 83,
    K_SIDE_RIGHT_6         = 84,
    K_BACKSPACE            = 85,
    K_END                  = 86,
    K_SIDE_LEFT_7          = 87,
    K_SIDE_RIGHT_7         = 88,
    K_RIGHT_SQUARE_BRACKET = 89,
    K_RIGHT_SHIFT          = 90,
    K_SIDE_LEFT_8          = 91,
    K_SIDE_RIGHT_8         = 92,
    K_BACKSLASH            = 93,
    K_UP                   = 94,
    K_LEFT                 = 95,
    K_ENTER                = 96,
    K_DOWN                 = 97,
};

// Time based printer
// static bool rgb_print(int keyCodes[], int count, int loopLength, effect_params_t* params) {
//     RGB_MATRIX_USE_LIMITS(led_min, led_max);

//     uint16_t time = (scale16by8(g_rgb_timer, rgb_matrix_config.speed) / 64);
//     if (loopLength > 0) {
//         time %= loopLength;
//     }

//     RGB rgb = rgb_matrix_hsv_to_rgb(rgb_matrix_config.hsv);

//     for (uint8_t i = led_min; i < led_max; i++) {
//         if (time < count && i == keyCodes[time]) {
//             rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
//         } else {
//             rgb_matrix_set_color(i, 0x00, 0x00, 0x00);
//         }
//     }

//     return rgb_matrix_check_finished_leds(led_max);
// }

// Tick based printer
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

static bool dirk(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    rgb_print(led_min, led_max, (int[]){K_D, K_I, K_R, K_K}, 4, 8192, 1024, 16, 0);
    return rgb_matrix_check_finished_leds(led_max);
}

static HSV map_colors(int keyCode) {
    switch (keyCode) {
        case K_F:
        case K_J:
            return (HSV){0, 216, 255};
        case K_D:
        case K_K:
            return (HSV){147, 205, 255};
        case K_F1:
        case K_F2:
        case K_F3:
        case K_F4:
        case K_F5:
        case K_F6:
        case K_F7:
        case K_F8:
        case K_F9:
        case K_F10:
        case K_F11:
        case K_F12:
        case K_DELETE:
        case K_HOME:
        case K_END:
        case K_F13:
            return (HSV){139, 191, 255};
        case K_UP:
        case K_DOWN:
        case K_LEFT:
        case K_RIGHT:
            return (HSV){225, 145, 255};
        default:
            return rgb_matrix_config.hsv;
    }
}

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

static bool solid_reactive_simple_custom(effect_params_t *params) {
    return solid_reactive_simple_custom_base(0, params);
}

static bool solid_reactive_simple_custom_delay(effect_params_t *params) {
    return solid_reactive_simple_custom_base(1024, params);
}

#    define groupSize 4
struct GroupedActivation {
    HSV color;
    int group[groupSize];
};

#    define groupedActivationCount 5
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

static bool solid_reactive_simple_custom2(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    uint16_t max_tick = 65535 / rgb_matrix_config.speed;
    for (uint8_t i = led_min; i < led_max; i++) {
        HSV      color;
        uint16_t tick;

        int groupID = findLedGroupId(i);
        if (groupID == -1) {
            color = map_colors(i);

            // Reverse search to find most recent key hit
            tick = max_tick;
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
            tick = max_tick;
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

        uint16_t offset = scale16by8(tick, rgb_matrix_config.speed);
        HSV      hsv    = SOLID_REACTIVE_SIMPLE_math(color, offset);
        RGB      rgb    = hsv_to_rgb(hsv);

        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

static int parseDigit(int keyCode) {
    switch (keyCode) {
        case K_1:
            return 1;
        case K_2:
            return 2;
        case K_3:
            return 3;
        case K_4:
            return 4;
        case K_5:
            return 5;
        case K_6:
            return 6;
        case K_7:
            return 7;
        case K_8:
            return 8;
        case K_9:
            return 9;
        case K_0:
            return 0;
        default:
            return -1;
    }
}

typedef struct {
    int numbers[2];
    int operator;
} equation;

static equation parseEquation(void) {
    equation e;
    e.numbers[0] = 0;
    e.numbers[1] = 0;

    int currentNumberIndex      = 0;
    int currentNumberDigitCount = 0;

    for (int j = g_last_hit_tracker.count - 2; j >= 0; j--) {
        if (currentNumberIndex == 0 && currentNumberDigitCount == 0 && g_last_hit_tracker.index[j] == K_ENTER) continue;
        int digit = parseDigit(g_last_hit_tracker.index[j]);
        if (digit == -1) {
            if (currentNumberDigitCount == 0) {
                e.operator= - 1;
                return e;
            }

            // We found at least a number and a digit of another meaning this is the end of the current digit and can finish up
            if (currentNumberIndex > 0 && currentNumberDigitCount > 0) {
                return e;
            } else {
                if (g_last_hit_tracker.index[j] == K_EQUALS || g_last_hit_tracker.index[j] == K_MINUS || g_last_hit_tracker.index[j] == K_X || g_last_hit_tracker.index[j] == K_SLASH) {
                    e.operator= g_last_hit_tracker.index[j];
                    currentNumberIndex++;
                    currentNumberDigitCount = 0;
                } else {
                    e.operator= - 1;
                    return e;
                }
            }
        } else {
            e.numbers[currentNumberIndex] += digit * pow(10, currentNumberDigitCount);
            currentNumberDigitCount++;
        }
    }

    if (currentNumberIndex == 0 || currentNumberDigitCount == 0) {
        e.operator= - 1;
    }
    return e;
}

static int digitToKeyCode(char digit) {
    switch (digit) {
        case '1':
            return K_1;
        case '2':
            return K_2;
        case '3':
            return K_3;
        case '4':
            return K_4;
        case '5':
            return K_5;
        case '6':
            return K_6;
        case '7':
            return K_7;
        case '8':
            return K_8;
        case '9':
            return K_9;
        case '0':
            return K_0;
        case '-':
            return K_MINUS;
        default:
            return -1;
    }
}

int  printBuffer[8];
int  printBufferSize = 0;
bool printMode       = false;

static bool calculator(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    if (printMode) {
        if (rgb_print(led_min, led_max, printBuffer, printBufferSize, 8192, 2048, 0, printBufferSize)) {
            printMode = false;
        }
        return rgb_matrix_check_finished_leds(led_max);
    }

    uint16_t max_tick = 65535 / rgb_matrix_config.speed;
    for (uint8_t i = led_min; i < led_max; i++) {
        uint16_t tick  = max_tick;
        int      index = -1;
        // Reverse search to find most recent key hit
        for (int8_t j = g_last_hit_tracker.count - 1; j >= 0; j--) {
            if (g_last_hit_tracker.index[j] == i && g_last_hit_tracker.tick[j] < tick) {
                tick  = g_last_hit_tracker.tick[j];
                index = g_last_hit_tracker.index[j];
                break;
            }
        }

        if (index == K_ENTER && g_last_hit_tracker.count >= 4) {
            int result = -1;

            // int first = parseDigit(g_last_hit_tracker.index[g_last_hit_tracker.count - 4]);
            // int second = parseDigit(g_last_hit_tracker.index[g_last_hit_tracker.count - 2]);
            // switch (g_last_hit_tracker.index[g_last_hit_tracker.count - 3]) {

            equation eq = parseEquation();

            if (eq.operator!= - 1) {
                switch (eq.operator) {
                    case K_EQUALS:
                        result = eq.numbers[1] + eq.numbers[0];
                        break;
                    case K_MINUS:
                        result = eq.numbers[1] - eq.numbers[0];
                        break;
                    case K_X:
                        result = eq.numbers[1] * eq.numbers[0];
                        break;
                    case K_SLASH:
                        result = eq.numbers[1] / eq.numbers[0];
                        break;
                }

                // for (int j = log10(result) + 1; result > 0; j--) {
                //     printBuffer[j] = digitToKeyCode(result % 10);
                //     printBufferSize++;

                //     result /= 10;
                // }
                if (result == 0) {
                    printBuffer[0]  = K_0;
                    printBufferSize = 1;
                } else {
                    int digitCount = (int)((floor(log10(abs(result))) + 1) * sizeof(char));
                    if (result < 0) digitCount++;
                    char resultString[digitCount];
                    sprintf(resultString, "%d", result);
                    for (int j = 0; j < digitCount; j++) {
                        printBuffer[j] = digitToKeyCode(resultString[j]);
                    }
                    printBufferSize = digitCount;
                }

                printTick = 0;
                printMode = true;
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

/*
uint32_t simon_says_tick = 0;

// The tick since the last recording phase started
uint32_t recording_start = 0;
// The tick since the last repeating phase started
uint32_t repeating_start = 2147483647;

uint32_t simon_says_delay = 0;
int length_of_recording = 0;

static bool simon_says(effect_params_t* params) {
    if (simon_says_delay == 0) {
        int ticks_since_last_key = g_last_hit_tracker.tick[g_last_hit_tracker.count - 1];
        int ticks_since_first_recording_tick = simon_says_tick - recording_start;

        uint16_t max_tick = ticks_since_first_recording_tick;
        uint16_t tick = max_tick;
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
        if (ticks_since_last_key > 65535 / rgb_matrix_config.speed
         && ticks_since_last_key < ticks_since_first_recording_tick
         ) {
            simon_says_delay = firstKeyTick;
            repeating_start = simon_says_tick;
            // length_of_recording = repeating_start - firstKeyTick;
            // length_of_recording = 1024;
            length_of_recording = simon_says_tick - firstKeyTick;
        }
    } else {
        // Stop repeating when we're done
        if (simon_says_tick >= repeating_start + length_of_recording) {
            simon_says_delay = 0;
            recording_start = simon_says_tick;
        }
    }

    simon_says_tick += 2;
    return solid_reactive_simple_custom_base(simon_says_delay, params);
}
*/

/*
static bool held_keys(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    for (uint8_t i = led_min; i < led_max; i++) {
        if (g_held_keys_tracker[i]) {
            // HSV color = map_colors(i);

            // uint16_t offset = scale16by8(tick, rgb_matrix_config.speed);
            // HSV      hsv    = SOLID_REACTIVE_SIMPLE_math(color, offset);
            // RGB      rgb    = hsv_to_rgb(hsv);
            rgb_matrix_set_color(i, 0xff, 0xff, 0xff);
        } else {
            rgb_matrix_set_color(i, 0x00, 0x00, 0x00);
        }
    }

    return rgb_matrix_check_finished_leds(led_max);
}
*/

// static bool held_keys_fade_out(effect_params_t* params) {
//     RGB_MATRIX_USE_LIMITS(led_min, led_max);

//     uint16_t max_tick = 65535 / rgb_matrix_config.speed;
//     for (uint8_t i = led_min; i < led_max; i++) {
//         //Reverse search to find most recent key-release or press of this key
//         uint16_t last_key_tick = max_tick;
//         bool last_key_pressed = false;
//         bool hit_found = false;
//         for (int8_t j = g_last_hit_tracker.count - 1; j >= 0; j--) {
//             if (g_last_hit_tracker.index[j] == i && g_last_hit_tracker.tick[j] < (last_key_tick)) {
//                 last_key_tick = g_last_hit_tracker.tick[j];
//                 last_key_pressed = g_last_hit_tracker.pressed[j];
//                 hit_found = true;
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

#endif
