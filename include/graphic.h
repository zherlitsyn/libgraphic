#ifndef GRAPHIC_H
#define GRAPHIC_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Public API of the graphic library: an immediate mode drawing layer
 * over Vulkan, GLFW and VMA.
 *
 * Deliberately free of any Vulkan header dependency: every backend
 * handle lives behind an opaque context in src/graphic_backend.c
 */

/* ------------------------------------------------------------------ */
/* enumerations                                                       */
/* ------------------------------------------------------------------ */

enum graphic_window_flag {
    GRAPHIC_WINDOW_RESIZABLE         = 1 << 0,
    GRAPHIC_WINDOW_VSYNC             = 1 << 1,
    GRAPHIC_WINDOW_MSAA_4X           = 1 << 2,    /* creation only */
    GRAPHIC_WINDOW_VALIDATION        = 1 << 3,    /* creation only */
    GRAPHIC_WINDOW_FULLSCREEN        = 1 << 4,
    GRAPHIC_WINDOW_UNDECORATED       = 1 << 5,
    GRAPHIC_WINDOW_HIDDEN            = 1 << 6,
    GRAPHIC_WINDOW_MINIMIZED         = 1 << 7,
    GRAPHIC_WINDOW_MAXIMIZED         = 1 << 8,
    GRAPHIC_WINDOW_UNFOCUSED         = 1 << 9,
    GRAPHIC_WINDOW_TOPMOST           = 1 << 10,
    GRAPHIC_WINDOW_TRANSPARENT       = 1 << 11,    /* creation only */
    GRAPHIC_WINDOW_HIGHDPI           = 1 << 12,    /* creation only */
    GRAPHIC_WINDOW_MOUSE_PASSTHROUGH = 1 << 13,
    GRAPHIC_WINDOW_BORDERLESS        = 1 << 14,
};

/* ------------------------------------------------------------------ */
/* window                            (src/graphic_platform_glfw.c)    */
/* ------------------------------------------------------------------ */

void graphic_window_flags_set(uint32_t flags); /* before graphic_window() */
bool graphic_window(int width, int height, const char *title);
void graphic_window_close(void);
bool graphic_window_should_close(void);

/* ------------------------------------------------------------------ */
/* events, cursor and time           (src/graphic_platform_glfw.c)    */
/* ------------------------------------------------------------------ */

void graphic_events_poll(void);

double graphic_time_get(void);

/* ------------------------------------------------------------------ */
/* frame lifecycle                   (src/graphic_core.c)             */
/* ------------------------------------------------------------------ */

void graphic_drawing_begin(void);
void graphic_drawing_end(void);

#endif /* GRAPHIC_H */