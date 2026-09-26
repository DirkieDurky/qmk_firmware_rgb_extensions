#pragma once

#include "matrix.h"
#include "helpers/solid_reactive_simple_custom_base.h"   // which pulls in reactive_fade.h

// Presses are read straight from the matrix rather than from
// g_last_hit_tracker, because that tracker can never hold a sequence. It keeps
// at most LED_HITS_TO_REMEMBER (8) entries and ages them through a 16 bit
// millisecond counter, after which quantum/rgb_matrix/rgb_matrix.c drops them
// (rgb_task_timers). Once it is full a new hit no longer raises its `count`, so
// there is no way to notice that anything happened at all.
#define SIMON_SAYS_HITS 32

// How long the keyboard must sit idle before it replays what you typed. A fade
// only lasts at most SIMON_SAYS_FADE_MAX_MS, so triggering on that would fire in
// the middle of ordinary typing and then throw the recording away on the next
// keystroke. Two seconds of silence is unambiguously "the user stopped", which
// leaves room for a whole phrase.
#ifndef SIMON_SAYS_IDLE_MS
#    define SIMON_SAYS_IDLE_MS 2000
#endif

// Silence between the end of one replay and the start of the next. A loop runs
// until its last key has faded out, so this is the dark gap you actually see
// between repeats. The two settings are independent: SIMON_SAYS_IDLE_MS decides
// *when the performance starts* after you stop typing, this one only spaces out
// the repeats once it is under way. 0 runs the sequence back to back.
#ifndef SIMON_SAYS_LOOP_DELAY_MS
#    define SIMON_SAYS_LOOP_DELAY_MS SIMON_SAYS_IDLE_MS
#endif

// Bounds for the fade window, chosen separately from the shared
// REACTIVE_FADE_*_MS defaults because the slowest fade here has to finish before
// the replay starts, or a key you pressed last would still be dimming when the
// performance began. Tying the ceiling to the idle threshold keeps that true
// whatever you set SIMON_SAYS_IDLE_MS to.
#ifndef SIMON_SAYS_FADE_MIN_MS
#    define SIMON_SAYS_FADE_MIN_MS 100
#endif
#ifndef SIMON_SAYS_FADE_MAX_MS
#    define SIMON_SAYS_FADE_MAX_MS (SIMON_SAYS_IDLE_MS / 2)
#endif
STATIC_ASSERT(SIMON_SAYS_FADE_MIN_MS > 0 && SIMON_SAYS_FADE_MAX_MS > SIMON_SAYS_FADE_MIN_MS, "SIMON_SAYS_FADE_MAX_MS must exceed SIMON_SAYS_FADE_MIN_MS");

static uint32_t simon_says_hit_time[SIMON_SAYS_HITS];
static uint8_t  simon_says_hit_led[SIMON_SAYS_HITS];
static uint8_t  simon_says_keys[MATRIX_ROWS][MATRIX_COLS]; // key states of the previous frame

static uint8_t  simon_says_hit_count = 0;     // presses currently in the recording
static uint16_t simon_says_age[RGB_MATRIX_LED_COUNT]; // ms since each LED was lit, UINT16_MAX = never
static uint16_t simon_says_fade     = SIMON_SAYS_FADE_MIN_MS; // ms for a key to dim to black
static uint32_t simon_says_now     = 0;        // timer_read32() for the frame being rendered
static uint32_t simon_says_next    = 0;        // earliest time the next replay may start
static uint32_t simon_says_started = 0;        // when the current replay began
static bool     simon_says_playing = false;    // true while replaying

static void simon_says_paint(uint8_t led, uint32_t age) {
    // A key older than the window is black, which is the common case here, and
    // SOLID_REACTIVE_SIMPLE_math(hsv, 255) would produce (0,0,0) too - so this is
    // exact, and it keeps the work below off the hot path.
    if (age >= simon_says_fade) {
        rgb_matrix_set_color(led, 0, 0, 0);
        return;
    }

    HSV hsv = SOLID_REACTIVE_SIMPLE_math(map_colors(led), reactive_fade_offset(age, simon_says_fade));
    RGB rgb = hsv_to_rgb(hsv);
    rgb_matrix_set_color(led, rgb.r, rgb.g, rgb.b);
}

static void simon_says_record(uint8_t led, uint32_t time) {
    if (simon_says_hit_count == SIMON_SAYS_HITS) {
        // Keep the most recent SIMON_SAYS_HITS presses and drop the oldest.
        memmove(&simon_says_hit_led[0], &simon_says_hit_led[1], SIMON_SAYS_HITS - 1);
        memmove(&simon_says_hit_time[0], &simon_says_hit_time[1], (SIMON_SAYS_HITS - 1) * sizeof(uint32_t));
        simon_says_hit_count--;
    }

    simon_says_hit_led[simon_says_hit_count]  = led;
    simon_says_hit_time[simon_says_hit_count] = time;
    simon_says_hit_count++;

    simon_says_next    = time + SIMON_SAYS_IDLE_MS;
}

static void simon_says_forget(void) {
    simon_says_hit_count = 0;
    simon_says_playing   = false;
    simon_says_next      = simon_says_now + SIMON_SAYS_IDLE_MS;
}

// Log every key that went down during this frame. On the first frame the shadow
// is seeded from the matrix instead, so keys that were already held when the
// effect got switched on are not mistaken for fresh presses.
static void simon_says_scan_keys(bool init) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        matrix_row_t state = matrix_get_row(row);

        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            bool down = (state & ((matrix_row_t)1 << col)) != 0;
            bool held = simon_says_keys[row][col] != 0;
            simon_says_keys[row][col] = down;

            // Only a 0 -> 1 transition is a press. Releases must not be logged,
            // or a held key would light up twice during the replay.
            if (init || !down || held) continue;

            uint8_t led[LED_HITS_TO_REMEMBER];
            uint8_t led_count = rgb_matrix_map_row_column_to_led(row, col, led);
            if (led_count == 0) continue;

            // The user is typing: a running replay is abandoned and the sequence
            // is recorded again from this keystroke on.
            if (simon_says_playing) simon_says_forget();
            for (uint8_t i = 0; i < led_count; i++) {
                simon_says_record(led[i], simon_says_now);
            }
        }
    }
}

// Advance the state machine and work out how long ago each LED was lit. Only
// valid on the first invocation of a frame, see simon_says() below.
static void simon_says_step(bool init) {
    if (init) simon_says_forget();
    simon_says_scan_keys(init);

    // Read once a frame: the speed can change under us mid-frame, and the
    // renderer and the loop bookkeeping below must agree on the window.
    uint16_t fade = reactive_fade_window(rgb_matrix_config.speed, SIMON_SAYS_FADE_MIN_MS, SIMON_SAYS_FADE_MAX_MS);
    simon_says_fade = fade;

    // The replay is over once every recorded key has been shown and the last one
    // has faded out. Looping keeps the keyboard performing the last sequence
    // until a key is pressed again.
    if (simon_says_playing && simon_says_hit_count > 0) {
        uint32_t length = (simon_says_hit_time[simon_says_hit_count - 1] - simon_says_hit_time[0]) + fade;
        if ((uint32_t)(simon_says_now - simon_says_started) >= length) {
            simon_says_playing = false;
            // Schedule the repeat from the *start* of this loop, not from now, so
            // the gap is exactly SIMON_SAYS_LOOP_DELAY_MS and the period does not
            // drift by a frame on every pass.
            simon_says_next = simon_says_started + length + SIMON_SAYS_LOOP_DELAY_MS;
        }
    }

    // Signed compare so this stays correct across the 32 bit timer wrap.
    if (!simon_says_playing && simon_says_hit_count > 0 && (int32_t)(simon_says_now - simon_says_next) >= 0) {
        simon_says_playing = true;
        simon_says_started = simon_says_next;
    }

    // Age of each LED, built once per frame and then reused by the render passes.
    uint32_t elapsed = simon_says_playing ? (uint32_t)(simon_says_now - simon_says_started) : 0;
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        simon_says_age[i] = UINT16_MAX;
    }

    for (uint8_t k = 0; k < simon_says_hit_count; k++) {
        uint32_t time = simon_says_hit_time[k];
        if (simon_says_playing) {
            // Shift the recording so that its first press happens right now.
            // That reproduces the original gaps between the keys exactly.
            uint32_t offset = time - simon_says_hit_time[0];
            if (elapsed < offset) continue; // not due yet
            time = simon_says_started + offset;
        }

        uint8_t  led = simon_says_hit_led[k];
        uint32_t age = simon_says_now - time;
        // The same key can be pressed more than once; the most recent press is
        // the one on screen.
        if (age < simon_says_age[led]) simon_says_age[led] = (uint16_t)age;
    }
}

static bool simon_says(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    // The effect is invoked once per RGB_MATRIX_LED_PROCESS_LIMIT chunk, i.e.
    // several times per frame, and params->iter counts those chunks. Only the
    // first one (iter == 0) starts a new frame, so all bookkeeping happens there,
    // exactly once per frame.
    if (params->iter == 0) {
        simon_says_now = timer_read32();
        simon_says_step(params->init);
    }

    for (uint8_t i = led_min; i < led_max; i++) {
        simon_says_paint(i, simon_says_age[i]);
    }
    return rgb_matrix_check_finished_leds(led_max);
}
