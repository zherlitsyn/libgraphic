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

/*
 * The uncompressed set. Compressed formats are named so that loaders
 * can report them, but nothing here can decode one: the size and the
 * pixel accessors return zero for those.
 */
enum graphic_pixel_format {
    GRAPHIC_PIXEL_UNCOMPRESSED_GRAYSCALE = 1,    /* 8 bit */
    GRAPHIC_PIXEL_UNCOMPRESSED_GRAY_ALPHA,       /* 8 + 8 */
    GRAPHIC_PIXEL_UNCOMPRESSED_R5G6B5,           /* 16 bit */
    GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8,           /* 24 bit */
    GRAPHIC_PIXEL_UNCOMPRESSED_R5G5B5A1,         /* 16 bit */
    GRAPHIC_PIXEL_UNCOMPRESSED_R4G4B4A4,         /* 16 bit */
    GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8A8,         /* 32 bit */
    GRAPHIC_PIXEL_UNCOMPRESSED_R32,              /* float */
    GRAPHIC_PIXEL_UNCOMPRESSED_R32G32B32,
    GRAPHIC_PIXEL_UNCOMPRESSED_R32G32B32A32,
    GRAPHIC_PIXEL_UNCOMPRESSED_R16,              /* half float */
    GRAPHIC_PIXEL_UNCOMPRESSED_R16G16B16,
    GRAPHIC_PIXEL_UNCOMPRESSED_R16G16B16A16,
    GRAPHIC_PIXEL_COMPRESSED_DXT1_RGB,
    GRAPHIC_PIXEL_COMPRESSED_DXT1_RGBA,
    GRAPHIC_PIXEL_COMPRESSED_DXT3_RGBA,
    GRAPHIC_PIXEL_COMPRESSED_DXT5_RGBA,
    GRAPHIC_PIXEL_COMPRESSED_ETC1_RGB,
    GRAPHIC_PIXEL_COMPRESSED_ETC2_RGB,
    GRAPHIC_PIXEL_COMPRESSED_ETC2_EAC_RGBA,
    GRAPHIC_PIXEL_COMPRESSED_PVRT_RGB,
    GRAPHIC_PIXEL_COMPRESSED_PVRT_RGBA,
    GRAPHIC_PIXEL_COMPRESSED_ASTC_4x4_RGBA,
    GRAPHIC_PIXEL_COMPRESSED_ASTC_8x8_RGBA,
};

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

/* ------------------------------------------------------------------ */
/* raw pixels                                                         */
/* ------------------------------------------------------------------ */

graphic_color_t graphic_pixel_color_get     (const void *source, int format);

void            graphic_pixel_color_set     (void           *destination,
                                             graphic_color_t color,
                                             int             format);

int             graphic_pixel_data_size_get (int width, int height, int format);

#endif /* GRAPHIC_COLOR_H */