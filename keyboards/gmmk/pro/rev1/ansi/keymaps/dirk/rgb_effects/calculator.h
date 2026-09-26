// The calculator: type a number, an operator and a number, press Enter, and the
// result is printed one digit at a time on the number row. The number row
// keycodes double as the LED indices of the keys that show them, which is how
// the answer gets drawn without a font - see digitToKeyCode().
//
// What is on screen the rest of the time is the display from
// helpers/held_keys.h: a key is lit for exactly as long as it is held. That is
// why the trigger below asks the matrix rather than g_last_hit_tracker.

#include "helpers/keycodes.h"
#include "helpers/rgb_print.h"
#include "helpers/map_colors.h"
#include "helpers/held_keys.h"

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

// Enter's level as of the previous frame, kept only so that its press can be
// found as an edge. The display already comes from helpers/held_keys.h and is
// level based - a key stays lit for as long as it is held - so the trigger is
// asked the same way, in calculator_enter_pressed() below.
static bool calculator_enter_held = false;

// Work out what has been typed and hand the answer to the printer.
static void calculator_evaluate(void) {
    // g_last_hit_tracker is what the equation itself is read from - it is the
    // only record of a *sequence* of keys, and it is the same one it was read
    // from before. Two things about it are relied on here. It holds at most
    // LED_HITS_TO_REMEMBER presses, newest last, and the newest one is the Enter
    // that has just been pressed: a number, an operator and a number plus that
    // Enter is the least there can be for an answer. And it cannot have missed
    // it, because the matrix scan that filled it runs before the RGB task that
    // reads it, in the same pass of keyboard_task.
    if (g_last_hit_tracker.count < 4) {
        return;
    }

    int result = -1;

    // int first = parseDigit(g_last_hit_tracker.index[g_last_hit_tracker.count - 4]);
    // int second = parseDigit(g_last_hit_tracker.index[g_last_hit_tracker.count - 2]);
    // switch (g_last_hit_tracker.index[g_last_hit_tracker.count - 3]) {

    equation eq = parseEquation();

    if (eq.operator== - 1) {
        return;
    }

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

// Was Enter pressed on this frame? Read off the matrix, through the same scan
// the display is drawn from, and not off g_last_hit_tracker, which cannot
// answer the question for either of the two reasons that matter now:
//
//   * It is never emptied. g_last_hit_tracker belongs to the RGB matrix, not to
//     this effect, and it still holds whatever was typed while some other effect
//     was running. "The newest entry is Enter" is therefore not "Enter was just
//     pressed": switching to the calculator long after typing an equation
//     elsewhere would find that old Enter still sitting at the end of the
//     buffer and work that equation out all over again. The answer used to be
//     gated on the hit being younger than the fade window, which is gone along
//     with the fade - and would have been a made up bound anyway, since
//     nothing fades here any more.
//
//   * It records presses and nothing else, so a key that is still down looks
//     exactly like one released a moment ago. Now that a held key stays lit, a
//     held Enter is an ordinary thing to be doing while reading your answer off
//     the keyboard, and it is not the moment to work it out.
//
// So this watches the rising edge on the switch itself. The shadow is the
// previous frame's level and exists only to find that edge; the level that was
// already there when this effect got selected is not a press, hence the init
// guard, so switching the effect on with Enter under your finger does not
// evaluate an equation you did not just finish. Note this also updates the
// shadow on every frame it is asked, including while a result is printing, so
// holding Enter across a print cannot leave it stale.
static bool calculator_enter_pressed(bool init) {
    bool down    = held_keys_switch_down(K_ENTER);
    bool pressed = down && !calculator_enter_held;

    calculator_enter_held = down;

    return pressed && !init;
}

static bool calculator(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    // The effect is invoked once per RGB_MATRIX_LED_PROCESS_LIMIT chunk, so
    // params->iter == 0 is the first chunk of a frame. Everything that is a
    // property of the frame rather than of an LED happens there, once: the
    // shared held_keys state machine is stepped - which is also what fills
    // held_keys_down for the chunks that follow - and Enter is asked whether it
    // was just pressed.
    if (params->iter == 0) {
        held_keys_step(params->init, held_keys_switch_down);

        // A press that lands while a result is on screen is ignored, as it was
        // before: the whole effect handed the LEDs to the printer until it was
        // done, so Enter during a print never worked anything out. Press it
        // again once the answer has finished.
        if (calculator_enter_pressed(params->init) && !printMode) {
            calculator_evaluate();
        }
    }

    // Printing owns the whole strip. The answer is drawn on the number row, the
    // same keys the digits were typed on, so there is nothing left to show of
    // the held keys for the length of it.
    if (printMode) {
        if (rgb_print(led_min, led_max, printBuffer, printBufferSize, 8192, 2048, 0, printBufferSize)) {
            printMode = false;
        }
        return rgb_matrix_check_finished_leds(led_max);
    }

    for (uint8_t i = led_min; i < led_max; i++) {
        held_keys_fade_paint(i, map_colors(i));
    }
    return rgb_matrix_check_finished_leds(led_max);
}
