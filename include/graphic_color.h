#ifndef GRAPHIC_COLOR_H
#define GRAPHIC_COLOR_H

#include <stdbool.h>
#include <stdint.h>

#include "graphic_linalg.h"

/*
 * Colours and pixel formats.
 *
 * graphic_color_t is always 8 bits per channel in sRGB, whatever the
 * format a buffer happens to be stored in. Conversion happens at the
 * edges, in graphic_pixel_color_get and graphic_pixel_color_set.
 */

typedef struct graphic_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} graphic_color_t;

/* ------------------------------------------------------------------ */
/* colour operations                                                  */
/* ------------------------------------------------------------------ */

bool graphic_color_is_equal                   (graphic_color_t a,
                                               graphic_color_t b);

graphic_color_t graphic_color_fade            (graphic_color_t color,
                                               float           alpha);

int graphic_color_to_int                      (graphic_color_t color);

vec4_t graphic_color_normalize                (graphic_color_t color);

graphic_color_t graphic_color_from_normalized (vec4_t normalized);

vec3_t graphic_color_to_hsv                   (graphic_color_t color);

graphic_color_t graphic_color_from_hsv        (float hue,
                                               float saturation,
                                               float value);

graphic_color_t graphic_color_tint            (graphic_color_t color,
                                               graphic_color_t tint);

graphic_color_t graphic_color_brightness      (graphic_color_t color,
                                               float           factor);

graphic_color_t graphic_color_contrast        (graphic_color_t color,
                                               float           contrast);

graphic_color_t graphic_color_alpha           (graphic_color_t color,
                                               float           alpha);

graphic_color_t graphic_color_alpha_blend     (graphic_color_t destination,
                                               graphic_color_t source,
                                               graphic_color_t tint);

graphic_color_t graphic_color_lerp            (graphic_color_t a,
                                               graphic_color_t b,
                                               float           factor);

graphic_color_t graphic_color_get             (unsigned int hex_value);

#endif /* GRAPHIC_COLOR_H */