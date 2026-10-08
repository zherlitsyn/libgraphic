#ifndef GRAPHIC_COLOR_H
#define GRAPHIC_COLOR_H

#include <stdbool.h>
#include <stdint.h>

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

#endif /* GRAPHIC_COLOR_H */