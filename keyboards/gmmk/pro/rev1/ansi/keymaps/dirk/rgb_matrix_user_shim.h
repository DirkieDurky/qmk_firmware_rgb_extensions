// clangd-only shim -- NOT part of the QMK build.
//
// `rgb_matrix_user.inc` is an include fragment: quantum/rgb_matrix/rgb_matrix.c
// `#include`s it (rgb_matrix.c:57) from the middle of a function body region,
// so on its own it is not a valid translation unit. This file reproduces the
// exact context the fragment sees at that point, so clangd can parse it with
// the file's real macro environment instead of guessing.
//
// Wired up by .clangd in this directory via `CompileFlags.Add: [-include, ...]`.
// Keep in sync with rgb_matrix.c lines 19-57 if upstream changes.
#pragma once

#include "rgb_matrix.h"
#include "progmem.h"
#include "eeconfig.h"
#include "keyboard.h"
#include "sync_timer.h"
#include "debug.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include <lib/lib8tion/lib8tion.h>

// rgb_matrix.c:31-35
#ifndef RGB_MATRIX_CENTER
const led_point_t k_rgb_matrix_center = {112, 32};
#else
const led_point_t k_rgb_matrix_center = RGB_MATRIX_CENTER;
#endif

// rgb_matrix.c:37-39
__attribute__((weak)) rgb_t rgb_matrix_hsv_to_rgb(hsv_t hsv) {
    return hsv_to_rgb(hsv);
}

// rgb_matrix.c:41-42 -- Generic effect runners
#include "rgb_matrix_runners.inc"

// rgb_matrix.c:46-47 -- both macros must be defined before the next include:
// RGB_MATRIX_EFFECT() expands to nothing, and it gates the custom-effect
// declarations inside the animation headers, which the fragment calls.
#define RGB_MATRIX_EFFECT(name)
#define RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// rgb_matrix.c:49-55 -- pulls in every animation header (and thus statics like
// SOLID_REACTIVE_SIMPLE_math) plus their #ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS
// declarations, all of which are in scope at the user include on line 57.
#include "rgb_matrix_effects.inc"
