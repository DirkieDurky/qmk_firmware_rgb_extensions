#pragma once

#include "matrix.h"
#include "timer.h"

#include "map_colors.h"
#include "reactive_fade.h"

// ---------------------------------------------------------------------------
// Shared "light the keys you are holding, then fade them out when you let go"
// state machine, and the two effects that are only that and nothing else.
//
// Everything that reacts to typing in this directory goes through this instead
// of leaning on g_last_hit_tracker, because that tracker records presses only:
// it appends an entry per hit, ages each one through a 16 bit millisecond
// counter, drops it on overflow (rgb_task_timers), and has no release event at
// all. So for a key that is still held down it cannot tell the key apart from
// one released a moment ago - which is the whole distinction these effects exist
// to draw. The matrix is read directly instead: matrix_get_row() is a plain read
// of the buffer the matrix scan has already filled, so it is both authoritative
// and cheap; simon_says.h reads it the same way for the same reason.
//
// An effect takes part by calling held_keys_step() on the first chunk of a
// frame, then asking what each of its LEDs should look like with
// held_keys_fade_paint() (or held_keys_fade_offset(), if it wants to do its own
// colour handling). The window is the REACTIVE_FADE_*_MS range, i.e. the same
// speed slider the other effects in this directory use.
// ---------------------------------------------------------------------------

// A switch can drive more than one LED: rgb_matrix_map_row_column_to_led_kb()
// lets a keyboard override the mapping, and the LED that g_led_config.matrix_co
// has for the switch is then appended to whatever that hook returned
// (quantum/rgb_matrix/rgb_matrix.c). This keyboard has no such override, so a
// switch always maps to exactly one LED and led_count is 1. The buffer is sized
// for the general case anyway, because the alternative is a stack smash.
#define HELD_KEYS_LED_BUFFER 8
STATIC_ASSERT(HELD_KEYS_LED_BUFFER >= 2, "must hold both the kb hook's result and the matrix_co LED");

// What the last scan saw at this LED. Only refreshed by held_keys_scan() on the
// first chunk of a frame, for the same reason simon_says.h keeps
// simon_says_age[]: the effect is invoked once per
// RGB_MATRIX_LED_PROCESS_LIMIT chunk (~20 LEDs, so 5 times a frame at this
// keyboard's 98), and each chunk only renders its own slice. Sampling per chunk
// would show the first chunks of a frame rendering the state of the scan they
// took themselves, and the last chunks the state of the scan before that.
static uint8_t held_keys_down[RGB_MATRIX_LED_COUNT];

static void held_keys_scan(void) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        matrix_row_t state = matrix_get_row(row);

        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            bool down = (state & ((matrix_row_t)1 << col)) != 0;

            // Proxies and unlit positions map to nothing, and then there is
            // simply nothing to record for this switch.
            uint8_t led[HELD_KEYS_LED_BUFFER];
            uint8_t led_count = rgb_matrix_map_row_column_to_led(row, col, led);
            for (uint8_t i = 0; i < led_count; i++) {
                held_keys_down[led[i]] = down;
            }
        }
    }
}

// Is the switch under this keycap down right now? This is the plain matrix
// answer, and it is what most effects want: one light per switch, on exactly
// while the key is held. Effects that put several switches on one light -
// tetris_groups.h, which lights a whole F-row at a time - ask their own question
// instead, see held_keys_step() below.
static bool held_keys_switch_down(uint8_t led) {
    return held_keys_down[led] != 0;
}

// The level on its own is not enough to drive a fade: it says a key is up, but
// not whether it was released a millisecond ago or has been up since boot, and
// the fade has to start at the release. Keeping the shadow and the fade in one
// byte rather than as two parallel arrays means the release edge and the fade it
// started are the same piece of state, so they cannot disagree.
enum {
    HELD_KEYS_UP     = 0, // up, and nothing to fade
    HELD_KEYS_DOWN   = 1, // held: full brightness
    HELD_KEYS_FADING = 2, // released, dimming towards black
};

static uint8_t  held_keys_state[RGB_MATRIX_LED_COUNT];
static uint32_t held_keys_release[RGB_MATRIX_LED_COUNT]; // timer_read32() at the release

// Read once per frame by the step below and reused by every chunk's paint, so
// all five chunks of a frame agree on the fade curve. Recomputing it per chunk
// would let the speed slider change mid-frame and show some LEDs at one
// brightness and others at another. Same reason as simon_says_fade.
static uint32_t held_keys_now    = 0;
static uint16_t held_keys_window = REACTIVE_FADE_MAX_MS;

// Advance the state machine, and read the matrix into held_keys_down. Only valid
// on the first invocation of a frame - see held_keys_fade_out() below for why
// that matters, and for why the window is read here rather than in the paint.
//
// down_fn answers "is this light on" for the LED, and is how an effect that
// shares one light between several switches takes part: it returns true while
// any of them is held, and the lights of that group then all turn on on the same
// frame and all start fading on the same frame, with no extra bookkeeping. Most
// effects pass held_keys_switch_down, i.e. the switch under the keycap.
//
// Sampling the level rather than watching for edges is what makes held_keys
// below correct when an effect is switched on while keys are already down. This
// one does need edges, so it keeps a shadow - but seeds that shadow from the
// matrix rather than trusting what is in it. The shadow describes whatever
// effect ran last, and its levels may be from before this one was selected: a
// key that was held at that moment and has since been released would otherwise
// look like a release edge and light up from full brightness for no reason.
// Seeding still leaves a key that is held right now at HELD_KEYS_DOWN, so it
// fades correctly when released.
static void held_keys_step(bool init, bool (*down_fn)(uint8_t led)) {
    held_keys_now    = timer_read32();
    held_keys_window = reactive_fade_window(rgb_matrix_config.speed, REACTIVE_FADE_MIN_MS, REACTIVE_FADE_MAX_MS);

    held_keys_scan();

    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        if (init) {
            held_keys_state[i] = down_fn(i) ? HELD_KEYS_DOWN : HELD_KEYS_UP;
            continue;
        }

        if (down_fn(i)) {
            // Pressed, or still held. Any fade in progress is abandoned here: a
            // later release starts a new one from that moment, so the stale
            // timestamp is never read.
            held_keys_state[i] = HELD_KEYS_DOWN;
        } else if (held_keys_state[i] == HELD_KEYS_DOWN) {
            // The falling edge, which is the release the fade counts from.
            held_keys_state[i]   = HELD_KEYS_FADING;
            held_keys_release[i] = held_keys_now;
        }
        // Otherwise unchanged: HELD_KEYS_FADING keeps dimming, HELD_KEYS_UP stays
        // black. A fade that has run past the window just keeps returning 255
        // below, so it costs one subtraction per chunk and needs no separate
        // "finished" state to retire into.
    }
}

// How bright this light should be, as the 0..255 offset
// SOLID_REACTIVE_SIMPLE_math consumes: 0 is the colour at full brightness, 255
// is black.
static uint16_t held_keys_fade_offset(uint8_t led) {
    switch (held_keys_state[led]) {
        case HELD_KEYS_DOWN:
            return 0; // SOLID_REACTIVE_SIMPLE_math leaves the colour alone

        case HELD_KEYS_FADING:
            // Unsigned subtraction, so this stays right across the 32 bit timer
            // wrap, the way simon_says_step's replay comparison does. The two
            // readings are at most a window apart anyway.
            return reactive_fade_offset(held_keys_now - held_keys_release[led], held_keys_window);

        case HELD_KEYS_UP:
        default:
            // Up with nothing to fade, i.e. not touched since this effect was
            // selected. A stale timestamp would also come out as 255 here, but
            // saying so explicitly keeps that from being a coincidence.
            return 255;
    }
}

// The usual way to ask: paint this LED in `color`, dimmed by the fade state.
static void held_keys_fade_paint(uint8_t led, HSV color) {
    RGB rgb = hsv_to_rgb(SOLID_REACTIVE_SIMPLE_math(color, held_keys_fade_offset(led)));
    rgb_matrix_set_color(led, rgb.r, rgb.g, rgb.b);
}

// ---------------------------------------------------------------------------
// held_keys: the state machine's output for the case where there is no fade, so
// a key is lit exactly while it is held. Reads the level rather than the state
// bytes, and so stays correct on its own when it is selected mid-hold.
// ---------------------------------------------------------------------------

static bool held_keys(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    // Stepped rather than just scanned, so the shared fade state stays in step
    // with the matrix for whoever asks next. Without it, switching straight from
    // this effect to one that fades would run that one against a stale shadow.
    if (params->iter == 0) {
        held_keys_step(params->init, held_keys_switch_down);
    }

    for (uint8_t i = led_min; i < led_max; i++) {
        if (held_keys_down[i]) {
            RGB rgb = hsv_to_rgb(map_colors(i));
            rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
        } else {
            rgb_matrix_set_color(i, 0x00, 0x00, 0x00);
        }
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// ---------------------------------------------------------------------------
// held_keys_fade_out: the shared state machine, and the simplest user of it -
// full brightness while down, then fade to black once the key is released.
// ---------------------------------------------------------------------------

static bool held_keys_fade_out(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    if (params->iter == 0) {
        held_keys_step(params->init, held_keys_switch_down);
    }

    for (uint8_t i = led_min; i < led_max; i++) {
        held_keys_fade_paint(i, map_colors(i));
    }
    return rgb_matrix_check_finished_leds(led_max);
}
