#include "helpers/keycodes.h"
#include "helpers/rgb_print.h"
#include "helpers/map_colors.h"
#include "helpers/reactive_fade.h"

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

    uint16_t window = reactive_fade_window(rgb_matrix_config.speed, REACTIVE_FADE_MIN_MS, REACTIVE_FADE_MAX_MS);
    for (uint8_t i = led_min; i < led_max; i++) {
        uint16_t tick  = window;
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

        uint16_t offset = reactive_fade_offset(tick, window);
        HSV      hsv    = SOLID_REACTIVE_SIMPLE_math(color, offset);
        RGB      rgb    = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}
