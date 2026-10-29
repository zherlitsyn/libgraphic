#ifndef GRAPHIC_H
#define GRAPHIC_H

#include <stdbool.h>
#include <stdint.h>

#include "graphic_linalg.h"
#include "graphic_color.h"
#include "graphic_camera.h"

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

enum graphic_draw_mode {
    GRAPHIC_DRAW_LINES = 1,
    GRAPHIC_DRAW_TRIANGLES,
    GRAPHIC_DRAW_QUADS,     /* expanded to triangles by index */
};

enum graphic_matrix_mode {
    GRAPHIC_MATRIX_MODELVIEW = 0,
    GRAPHIC_MATRIX_PROJECTION,
};

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

bool graphic_window(int         width,
                    int         height,
                    const char *title);

void graphic_window_close(void);

bool graphic_window_should_close(void);

bool graphic_window_is_fullscreen(void);
bool graphic_window_is_minimized(void);
bool graphic_window_is_resized(void);

void graphic_window_fullscreen_toggle(void);

void graphic_window_title_set(const char *title);

/* ------------------------------------------------------------------ */
/* screen and monitors               (src/graphic_platform_glfw.c)    */
/* ------------------------------------------------------------------ */

/*
 * Screen size is in window coordinates, render size is in pixels.
 * They differ on a HiDPI display; the render size is what the
 * swapchain and the projection matrix use.
 */
int graphic_screen_width_get(void);
int graphic_screen_height_get(void);
int graphic_render_width_get(void);
int graphic_render_height_get(void);

int graphic_monitor_current_get(void);

/* ------------------------------------------------------------------ */
/* events, cursor and time           (src/graphic_platform_glfw.c)    */
/* ------------------------------------------------------------------ */

void graphic_events_poll(void);

double graphic_time_get(void);

void graphic_time_wait(double seconds);

/* ------------------------------------------------------------------ */
/* frame lifecycle                   (src/graphic_core.c)             */
/* ------------------------------------------------------------------ */

void graphic_drawing_begin(void);
void graphic_drawing_end(void);

/*
 * graphic_drawing_end() is the sum of these three plus the frame
 * limiter. Call them yourself instead of it when you want full
 * control over the shape of the frame.
 */
void graphic_screen_buffer_swap(void); /* record, submit, present */
void graphic_background_clear(graphic_color_t color);

void graphic_mode3d_begin(graphic_camera3d_t camera);
void graphic_mode3d_end(void);

/* ------------------------------------------------------------------ */
/* immediate mode geometry           (src/graphic.c)                  */
/* ------------------------------------------------------------------ */

void graphic_matrix_mode(int mode); /* enum graphic_matrix_mode */
void graphic_matrix_push(void);
void graphic_matrix_pop(void);
void graphic_matrix_identity(void);
void graphic_matrix_load(mat4_t matrix);
void graphic_matrix_multiply(mat4_t matrix);

void graphic_translatef(float x,
                        float y,
                        float z);

void graphic_rotatef(float angle, /* degrees */
                     float x,
                     float y,
                     float z); 

void graphic_scalef(float x,
                    float y,
                    float z);

void graphic_begin(int mode); /* enum graphic_draw_mode */
void graphic_end(void);

void graphic_color4ub(uint8_t r,
                      uint8_t g,
                      uint8_t b,
                      uint8_t a);

void graphic_color4f(float r,
                     float g,
                     float b,
                     float a);

void graphic_normal3f(float x,
                      float y,
                      float z);

void graphic_texcoord2f(float u,
                        float v);

void graphic_vertex3f(float x,
                      float y,
                      float z);

void graphic_vertex2f(float x,
                      float y);

#endif /* GRAPHIC_H */