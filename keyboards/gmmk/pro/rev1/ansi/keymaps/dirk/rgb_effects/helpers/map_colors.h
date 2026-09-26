#pragma once

#include "keycodes.h"

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
