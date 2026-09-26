#pragma once

#include "matrix.h"
#include "helpers/solid_reactive_simple_custom_base.h" // which pulls in reactive_fade.h
#include "helpers/held_keys.h"

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

// How long a repeat holds the light up for a press that has no release to
// measure, which happens in one case only: a key the user is still holding when
// the performance starts, because the recording stops taking edits from that
// point on (simon_says_release). Sealing it with however long they have held it
// by then would let one forgotten key stretch a single loop to minutes, and it
// would never be released after that either, so the loop would never come round
// again - a long press is the safe reading of "they have not let go of this".
//
// Tied to the idle threshold because that is already how long such a key has
// demonstrably been held: the performance cannot start before SIMON_SAYS_IDLE_MS
// after the last keystroke, and a press recorded before that one has been down
// for at least that long.
#ifndef SIMON_SAYS_HOLD_MAX_MS
#    define SIMON_SAYS_HOLD_MAX_MS SIMON_SAYS_IDLE_MS
#endif

// Sentinels for the two arrays below, which have to be values no real entry can
// take: an index into the recording, and a 32 bit timestamp.
#define SIMON_SAYS_UP_UNSET UINT32_MAX // press is still down, no release recorded
#define SIMON_SAYS_NO_HIT UINT8_MAX    // replay has nothing on this LED

// A press is recorded as the pair of edges that makes a keypress:
// simon_says_hit_time is when the key went down, simon_says_hit_up when it came
// back up. Both are needed to repeat one faithfully, because a keypress is not
// an instant: the light is on for the whole hold and only starts fading at the
// release, which is what helpers/held_keys.h draws for a key under your finger
// and therefore what a repeat has to reproduce. The release is the falling edge
// of the same matrix scan that finds the press, see simon_says_scan_keys().
static uint32_t simon_says_hit_time[SIMON_SAYS_HITS];
static uint32_t simon_says_hit_up[SIMON_SAYS_HITS]; // SIMON_SAYS_UP_UNSET while the key is still down
static uint8_t  simon_says_hit_led[SIMON_SAYS_HITS];
static uint8_t  simon_says_keys[MATRIX_ROWS][MATRIX_COLS]; // key states of the previous frame

static uint8_t  simon_says_hit_count = 0;                    // presses currently in the recording
static uint8_t  simon_says_show[RGB_MATRIX_LED_COUNT];       // press on each LED this frame, SIMON_SAYS_NO_HIT = none
static uint16_t simon_says_fade    = SIMON_SAYS_FADE_MIN_MS; // ms for a key to dim to black
static uint32_t simon_says_now     = 0;                      // timer_read32() for the frame being rendered
static uint32_t simon_says_next    = 0;                      // earliest time the next replay may start
static uint32_t simon_says_started = 0;                      // when the current replay began
static bool     simon_says_playing = false;                  // true while replaying

// True while the buffer holds a phrase the user is still typing, so the next
// keystroke extends it. Without this the other two states are indistinguishable:
// a non-empty buffer that is not currently playing is either a half-typed
// phrase (append) or a finished performance waiting out its dark gap before the
// next repeat (discard), and there is no other bit that tells them apart.
static bool simon_says_typing = false;

// How far into the phrase a recorded press belongs, in ms from the first one.
// This is the gap the user left between the two keys, kept as measured, because
// the replay shifts the whole recording by one amount (see simon_says_started)
// and every press moves by the same amount with it.
static uint32_t simon_says_offset(uint8_t hit) {
    return simon_says_hit_time[hit] - simon_says_hit_time[0];
}

// How long the repeat keeps this press lit before the fade may start, i.e. the
// hold the user actually gave the key.
static uint32_t simon_says_hold(uint8_t hit) {
    if (simon_says_hit_up[hit] == SIMON_SAYS_UP_UNSET) return SIMON_SAYS_HOLD_MAX_MS; // still down, see above
    return simon_says_hit_up[hit] - simon_says_hit_time[hit];
}

static void simon_says_paint(uint8_t led) {
    uint8_t hit = simon_says_show[led];
    if (hit == SIMON_SAYS_NO_HIT) {
        rgb_matrix_set_color(led, 0, 0, 0);
        return;
    }

    // Where the replay of this press is in the press' own timeline: on from 0,
    // still held at the length of the hold, and past that into the fade. Same
    // two phases a real key has, and the same order - the release starts the
    // fade, rather than the press, which is what the live path below shows and
    // what the recorded hold has to reproduce.
    uint32_t due   = simon_says_started + simon_says_offset(hit);
    uint32_t since = simon_says_now - due;
    uint32_t hold  = simon_says_hold(hit);
    uint32_t age   = since < hold ? 0 : since - hold;

    // A key past the end of its fade is black, which is the common case here, and
    // SOLID_REACTIVE_SIMPLE_math(hsv, 255) would produce (0,0,0) too - so this is
    // exact, and it keeps the work below off the hot path. An unreached fade
    // (age == 0, i.e. still held) takes the other branch and comes out at full
    // brightness, which is what held_keys.h draws for a pressed key.
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
        // Keep the most recent SIMON_SAYS_HITS presses and drop the oldest, its
        // release included. A press that is still down is therefore dropped
        // whole, rather than leaving an entry behind that no later release could
        // ever close.
        memmove(&simon_says_hit_led[0], &simon_says_hit_led[1], SIMON_SAYS_HITS - 1);
        memmove(&simon_says_hit_time[0], &simon_says_hit_time[1], (SIMON_SAYS_HITS - 1) * sizeof(uint32_t));
        memmove(&simon_says_hit_up[0], &simon_says_hit_up[1], (SIMON_SAYS_HITS - 1) * sizeof(uint32_t));
        simon_says_hit_count--;
    }

    simon_says_hit_led[simon_says_hit_count]  = led;
    simon_says_hit_time[simon_says_hit_count] = time;
    simon_says_hit_up[simon_says_hit_count]   = SIMON_SAYS_UP_UNSET; // closed by simon_says_release below
    simon_says_hit_count++;

    simon_says_typing = true; // user input, not a finished performance
    simon_says_next   = time + SIMON_SAYS_IDLE_MS;
}

// The release closes the press recorded for this light, and its timestamp is
// what the repeat fades from - so the light comes up when the real key did, not
// when it went down. The newest entry for that light is the one to close, and
// searching backwards for it is what keeps that true after the buffer has been
// shifted by a full one: the shadow in simon_says_scan_keys() only reports a
// press when the key was up before, so the press that is down right now cannot
// have been closed already.
static void simon_says_release(uint8_t led, uint32_t time) {
    // Only a phrase the user is still typing takes edits. Once a performance is
    // under way the recording is a finished artifact: what is on screen is the
    // replay, and a key let go during it - or during the dark gap between two
    // repeats - has nothing to do with the phrase being performed, and writing
    // its release in would make the phrase grow a hold in the middle of the
    // loop. A keystroke is still picked up, because that starts a new recording,
    // which forgets the old one.
    if (!simon_says_typing) return;

    for (int8_t k = simon_says_hit_count - 1; k >= 0; k--) {
        if (simon_says_hit_led[k] != led) continue;
        simon_says_hit_up[k] = time;
        return;
    }

    // Nothing to close: the key was already down when this effect got switched
    // on, or the phrase was forgotten while it was held, so it was never
    // recorded as a press. Both leave the recording alone, and neither shows up
    // in a repeat, which is right - the repeat starts from what was typed.
}

static void simon_says_forget(void) {
    simon_says_hit_count = 0;
    simon_says_playing   = false;
    simon_says_typing    = false;
    simon_says_next      = simon_says_now + SIMON_SAYS_IDLE_MS;
}

// Log every key that went down during this frame, and note every key that came
// back up, because a repeat needs both halves of the press. On the first frame
// the shadow is seeded from the matrix instead, so keys that were already held
// when the effect got switched on are not mistaken for fresh presses.
static void simon_says_scan_keys(bool init) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        matrix_row_t state = matrix_get_row(row);

        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            bool down                 = (state & ((matrix_row_t)1 << col)) != 0;
            bool held                 = simon_says_keys[row][col] != 0;
            simon_says_keys[row][col] = down;

            // Both edges and nothing else. A key that stayed the way it was
            // neither opens a press nor closes one, and the level itself is
            // already on screen through helpers/held_keys.h.
            if (init || down == held) continue;

            uint8_t led[LED_HITS_TO_REMEMBER];
            uint8_t led_count = rgb_matrix_map_row_column_to_led(row, col, led);
            if (led_count == 0) continue;

            if (!down) {
                // The falling edge. Logging a release as a press of its own
                // would light the key up twice in the replay, and the press it
                // belongs to is the entry that gets closed instead.
                for (uint8_t i = 0; i < led_count; i++) {
                    simon_says_release(led[i], simon_says_now);
                }
                continue;
            }

            // Any keystroke that is not continuing a phrase the user is still
            // typing starts a fresh recording, discarding whatever was in the
            // buffer. That has to cover the dark gap between two repeats and
            // not just a replay in progress: the buffer is still full then, so
            // keying during the gap appended to the finished sequence and the
            // new phrase was played back as its tail.
            if (!simon_says_typing) simon_says_forget();
            for (uint8_t i = 0; i < led_count; i++) {
                simon_says_record(led[i], simon_says_now);
            }
        }
    }
}

// How long one pass of the phrase takes: every press held for as long as the
// user held it, and then faded out. It is not the last press that decides this -
// a long hold early in the phrase can still be lit after it - so the longest of
// them wins, which is what lets the loop below end on the last light going out
// rather than on the last key being touched.
static uint32_t simon_says_length(void) {
    uint32_t length = 0;
    for (uint8_t k = 0; k < simon_says_hit_count; k++) {
        uint32_t end = simon_says_offset(k) + simon_says_hold(k) + simon_says_fade;
        if (end > length) length = end;
    }
    return length;
}

// Advance the state machine and work out which recorded press each LED is
// showing. Only valid on the first invocation of a frame, see simon_says() below.
static void simon_says_step(bool init) {
    if (init) simon_says_forget();
    simon_says_scan_keys(init);

    // Read once a frame: the speed can change under us mid-frame, and the
    // renderer and the loop bookkeeping below must agree on the window.
    uint16_t fade   = reactive_fade_window(rgb_matrix_config.speed, SIMON_SAYS_FADE_MIN_MS, SIMON_SAYS_FADE_MAX_MS);
    simon_says_fade = fade;

    // The replay is over once every recorded press has been shown, held and
    // faded out. Looping keeps the keyboard performing the last sequence until a
    // key is pressed again.
    if (simon_says_playing && simon_says_hit_count > 0) {
        uint32_t length = simon_says_length();
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
        // From here on the buffer is a performance being replayed, not a phrase
        // being typed, so a keystroke now has to start over rather than append.
        simon_says_typing = false;
    }

    // Nothing below is read while the live path is painting, i.e. whenever the
    // board is waiting for the next phrase rather than performing one.
    if (!simon_says_playing) return;

    // Which press each LED is showing, built once per frame and then reused by
    // the render passes.
    uint32_t elapsed = (uint32_t)(simon_says_now - simon_says_started);
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        simon_says_show[i] = SIMON_SAYS_NO_HIT;
    }

    for (uint8_t k = 0; k < simon_says_hit_count; k++) {
        if (elapsed < simon_says_offset(k)) continue; // not due yet

        // Presses are recorded in order, so the last one due for a light is its
        // most recent press, and it simply takes over: the same key pressed
        // twice shows the newer one rather than the older one still fading.
        simon_says_show[simon_says_hit_led[k]] = k;
    }
}

static bool simon_says(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    // The effect is invoked once per RGB_MATRIX_LED_PROCESS_LIMIT chunk, i.e.
    // several times per frame, and params->iter counts those chunks. Only the
    // first one (iter == 0) starts a new frame, so all bookkeeping happens there,
    // exactly once per frame.
    if (params->iter == 0) {
        // Stepped even when a replay is under way and its output hides this, so
        // the state is already right for the moment the replay ends.
        held_keys_step(params->init, held_keys_switch_down);
        simon_says_now = timer_read32();
        simon_says_step(params->init);
    }

    for (uint8_t i = led_min; i < led_max; i++) {
        if (simon_says_playing) {
            // A replay has to keep fading on its own clock. The keys it lights
            // are not being pressed, so there is no release for a held-key fade
            // to start from, and the recorded times are what has to drive it.
            simon_says_paint(i);
        } else {
            // While you are typing, and while the board sits idle waiting to
            // start a performance, this is just the shared hold-to-light fade -
            // so a key you are still holding stays lit instead of dimming under
            // your finger, and keeps its colour until you let go. A repeat draws
            // the same two phases from the recording above, which is what keeps
            // what it shows you and what you just watched yourself do identical.
            held_keys_fade_paint(i, map_colors(i));
        }
    }
    return rgb_matrix_check_finished_leds(led_max);
}
