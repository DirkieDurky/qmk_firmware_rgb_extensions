#pragma once

#include <stdint.h>

// STATIC_ASSERT at the bottom comes from quantum/compiler_support.h, which
// rgb_matrix_types.h pulls in. Every effect in this directory is an include
// fragment of rgb_matrix_user.inc, which rgb_matrix.c includes after
// rgb_matrix.h, so it is always in scope - same as for the simon_says.h assert.

// How a "a key was pressed, light it up and then dim it" effect turns the
// RGB_MATRIX_SPEED slider into a fade. Every effect in this keymap that reacts
// to key hits needs the same two things:
//
//   1. how many milliseconds a key takes to go from full brightness to black,
//      picked by the speed slider
//   2. how to turn "how long ago was this lit" into the 0..255 offset that
//      SOLID_REACTIVE_SIMPLE_math() consumes (0 = full brightness, 255 = black)
//
// Both used to be written inline, in four places, as
//
//     uint16_t max_tick = 65535 / rgb_matrix_config.speed;
//     uint16_t offset   = scale16by8(tick, rgb_matrix_config.speed);
//
// which is wrong in four ways:
//
//   * 65535/0 divides by zero. Cortex-M has no divide-by-zero trap enabled, so
//     it returns 0, max_tick becomes 0, the offset becomes
//     scale16by8(0, 0) = 0, and SOLID_REACTIVE_SIMPLE_math computes
//     v = scale8(255 - 0, v) = v. Every LED then renders at full colour
//     permanently: the board floods solid and nothing ever fades. Upstream QMK
//     dodges this with qadd8(speed, 1) in effect_runner_reactive.h.
//
//   * The direction was backwards. 65535 / speed grows with speed, so turning
//     the knob up made a key linger *longer* - the opposite of what "speed" says
//     on the label, and the opposite of what QMK's own non-reactive runners do.
//
//   * 1/speed is the wrong curve for a slider people read as linear. The top
//     half of the travel (128..255, where the 127 default sits and where you
//     actually work) spanned 511..257 ms, a 2x change over half the knob, while
//     the bottom half spanned 65535..516 ms, a 127x change. One step of the
//     slider was worth 67 ms of fade at speed 32, 4 ms at the default, and 1 ms
//     at 255 - coarse where you want fine and fine where you want coarse.
//
//   * The reachable window was wrong at both ends. 257 ms was the floor, so no
//     setting faded any faster than a quarter of a second, and speed 1 asked for
//     65.5 s, which is not a fade but a key stuck on - and it is longer than
//     g_last_hit_tracker.tick[], a uint16 that rgb_task_timers evicts on 16 bit
//     overflow, so the effects reading that tracker could not even render it.
//
// Worth noting that the two "speed" factors in the old form cancelled out:
// (65535/speed) * speed / 256 is always just under 256, so the offset did sweep
// 0..255 at every speed, and the fade did reach black. But only because
// 65535/speed truncates down - change either constant and it stops working.

// Default bounds for the fade window, in milliseconds. The slider picks a point
// between them. Override these before including this header, or per call site,
// if an effect wants a different range - and if you do, add the STATIC_ASSERT
// from the bottom of this file so an inverted pair is caught at build time
// instead of silently producing a garbage window.
#ifndef REACTIVE_FADE_MIN_MS
#    define REACTIVE_FADE_MIN_MS 100
#endif
#ifndef REACTIVE_FADE_MAX_MS
#    define REACTIVE_FADE_MAX_MS 1000
#endif

// Linear, bounded, monotonic mapping from the speed slider onto a fade window.
//
//   speed   0  -> max_ms   (slowest fade)
//   speed 255  -> min_ms   (fastest fade)
//
// Turning the knob up fades faster, because that is what "speed" means to
// anyone reading the label. Note that upstream QMK's *reactive* runners go the
// other way - max_tick = 65535 / speed in effect_runner_reactive.h, so there a
// higher setting leaves a key lit for longer - while its non-reactive runners
// (effect_runner_i, _dx_dy, _sin_cos_i) scale time by speed and agree with this
// one. Upstream is internally inconsistent; this follows the non-reactive
// direction, which is the one that matches the name of the setting.
//
// Because 255 is how many steps the slider has, one step of the slider is always
// worth (max_ms - min_ms) / 255 of fade, everywhere on the knob. That is the
// whole point of doing it this way rather than dividing.
static inline uint16_t reactive_fade_window(uint8_t speed, uint16_t min_ms, uint16_t max_ms) {
    // uint32_t intermediate: speed * (max_ms - min_ms) overflows a uint16_t.
    // Subtracted from max_ms rather than added to min_ms, so that raising the
    // speed shortens the window instead of lengthening it.
    return max_ms - (uint16_t)(((uint32_t)speed * (uint16_t)(max_ms - min_ms)) / 255);
}

// How far along the fade a key that was lit age_ms ago should be, as the 0..255
// offset for SOLID_REACTIVE_SIMPLE_math().
//
// This divides by the window rather than multiplying by the speed, so the
// dimming curve is linear in time at every slider setting, and the offset
// provably reaches 255 at the end of the fade instead of by the rounding
// coincidence described at the top of this file. It also cannot divide by zero,
// since a window of 0 is rejected by the STATIC_ASSERT at every call site.
static inline uint16_t reactive_fade_offset(uint32_t age_ms, uint16_t window_ms) {
    if (age_ms >= window_ms) return 255;   // fully faded; also keeps the multiply below in range
    return (uint16_t)((age_ms * 255) / window_ms);
}

STATIC_ASSERT(REACTIVE_FADE_MIN_MS > 0 && REACTIVE_FADE_MAX_MS > REACTIVE_FADE_MIN_MS, "REACTIVE_FADE_MAX_MS must exceed REACTIVE_FADE_MIN_MS");
