#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graphic_image.h"
#include "graphic_geometry.h"

/*
 * Decoding and encoding are separate: stb_image.h alone brings in PNG,
 * JPEG and the rest for loading, while writing anything but BMP also
 * needs stb_image_write.h.
 */
#ifdef GRAPHIC_USE_STB_IMAGE
    #define STB_IMAGE_IMPLEMENTATION
    #define STBI_NO_STDIO
    #include <stb_image.h>
#endif

#ifdef GRAPHIC_USE_STB_IMAGE_WRITE
    #define STB_IMAGE_WRITE_IMPLEMENTATION
    #include <stb_image_write.h>
#endif

/*
 * Every operation here goes through graphic_pixel_color_get and
 * graphic_pixel_color_set, so all uncompressed formats work without a
 * conversion pass. The cost is a few instructions per pixel, which is
 * the right trade for code that runs once at load time rather than
 * once per frame.
 */

static int pixel_size_get(int format)
{
    return graphic_pixel_data_size_get(1, 1, format);
}

static bool image_usable(const graphic_image_t *image)
{
    return image         != NULL &&
           image->data   != NULL &&
           image->width  >  0    &&
           image->height >  0    &&
           pixel_size_get(image->format) > 0;
}

static graphic_color_t image_pixel_read(const graphic_image_t *image,
                                        int                    x,
                                        int                    y)
{
    graphic_color_t clear = { 0, 0, 0, 0 };
    const uint8_t  *base;

    if (x < 0 || y < 0 || x >= image->width || y >= image->height)
        return clear;

    base = (const uint8_t *)image->data +
           ((size_t)y * (size_t)image->width +
            (size_t)x) * (size_t)pixel_size_get(image->format);

    return graphic_pixel_color_get(base, image->format);
}

static void image_pixel_write(graphic_image_t *image,
                              int              x,
                              int              y,
                              graphic_color_t  color)
{
    uint8_t *base;

    if (x < 0 || y < 0 || x >= image->width || y >= image->height)
        return;

    base = (uint8_t *)image->data +
           ((size_t)y * (size_t)image->width +
            (size_t)x) * (size_t)pixel_size_get(image->format);

    graphic_pixel_color_set(base, color, image->format);
}

static graphic_image_t image_blank(int width, int height)
{
    graphic_image_t image = { NULL, 0, 0, 0, 0 };
    int             size;

    if (width <= 0 || height <= 0)
        return image;

    size = graphic_pixel_data_size_get(width,
                                       height,
                                       GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8A8);
    if (size <= 0)
        return image;

    image.data = calloc(1, (size_t)size);
    if (image.data == NULL)
        return image;

    image.width   = width;
    image.height  = height;
    image.mipmaps = 1;
    image.format  = GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8A8;

    return image;
}

/* ------------------------------------------------------------------ */
/* basics                                                             */
/* ------------------------------------------------------------------ */

bool graphic_image_is_valid(graphic_image_t image)
{
    return image.data    != NULL &&
           image.width   >  0    &&
           image.height  >  0    &&
           image.mipmaps >  0    &&
           image.format  >  0;
}

void graphic_image_free(graphic_image_t image)
{
    free(image.data);
}

graphic_image_t graphic_image_copy(graphic_image_t image)
{
    graphic_image_t copy   = { NULL, 0, 0, 0, 0 };
    int             width  = image.width;
    int             height = image.height;

    if (!graphic_image_is_valid(image))
        return copy;

    /* the mip chain lives in the same allocation, so measure it all */
    int size = 0;
    for (int level = 0; level < image.mipmaps; level++) {
        size += graphic_pixel_data_size_get(width, height, image.format);

        width  /= 2;
        height /= 2;

        if (width < 1)
            width = 1;

        if (height < 1)
            height = 1;
    }

    copy.data = malloc((size_t)size);
    if (copy.data == NULL)
        return copy;

    memcpy(copy.data, image.data, (size_t)size);
    copy.width   = image.width;
    copy.height  = image.height;
    copy.mipmaps = image.mipmaps;
    copy.format  = image.format;

    return copy;
}

graphic_image_t graphic_image_from_image(graphic_image_t image,
                                         rect_t          rectangle)
{
    graphic_image_t result;

    result = graphic_image_copy(image);
    graphic_image_crop(&result, rectangle);

    return result;
}

/* channel 0 is red, 3 is alpha; the result is a grayscale image */
graphic_image_t graphic_image_from_channel(graphic_image_t image,
                                           int             channel)
{
    graphic_image_t result = { NULL, 0, 0, 0, 0 };

    if (!image_usable(&image) || channel < 0 || channel > 3)
        return result;

    result = image_blank(image.width, image.height);
    if (result.data == NULL)
        return result;

    graphic_image_format(&result, GRAPHIC_PIXEL_UNCOMPRESSED_GRAYSCALE);

    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            graphic_color_t source = image_pixel_read(&image, x, y);
            uint8_t         value;
            graphic_color_t gray;

            switch (channel) {
                case 0:
                    value = source.r;
                    break;
                case 1:
                    value = source.g;
                    break;
                case 2:
                    value = source.b;
                    break;
                default:
                    value = source.a;
                    break;
            }

            /*
             * The grayscale writer applies luminance weights,
             * so the value is placed in all three channels to
             * survive the round trip unchanged.
             */
            gray.r = value;
            gray.g = value;
            gray.b = value;
            gray.a = 255;

            image_pixel_write(&result, x, y, gray);
        }
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* format conversion                                                  */
/* ------------------------------------------------------------------ */

void graphic_image_format(graphic_image_t *image, int new_format)
{
    if (!image_usable(image) || new_format == image->format)
        return;

    int new_pixel_size = pixel_size_get(new_format);
    if (new_pixel_size <= 0)
        return; /* compressed target, nothing to do */

    int size = graphic_pixel_data_size_get(image->width, image->height, new_format);
    if (size <= 0)
        return; /* too big for the new format */

    uint8_t *converted = (uint8_t *)malloc((size_t)size);
    if (converted == NULL)
        return;

    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            graphic_color_t color = image_pixel_read(image, x, y);
            size_t offset = ((size_t)y * (size_t)image->width +
                             (size_t)x) * (size_t)new_pixel_size;

            graphic_pixel_color_set(converted + offset, color, new_format);
        }
    }

    free(image->data);
    image->data    = converted;
    image->format  = new_format;
    image->mipmaps = 1; /* the old chain is now meaningless */
}

/* ------------------------------------------------------------------ */
/* generation                                                         */
/* ------------------------------------------------------------------ */

graphic_image_t graphic_image_generate_color(int             width,
                                             int             height,
                                             graphic_color_t color)
{
    graphic_image_t image = image_blank(width, height);

    if (image.data == NULL)
        return image;

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            image_pixel_write(&image, x, y, color);

    return image;
}

graphic_image_t graphic_image_generate_gradient_linear(int             width,
                                                       int             height,
                                                       int             direction,
                                                       graphic_color_t start,
                                                       graphic_color_t end)
{
    graphic_image_t image = image_blank(width, height);

    if (image.data == NULL)
        return image;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float factor;

            if (direction == GRAPHIC_GRADIENT_VERTICAL)
                factor = (height > 1) ? (float)y / (float)(height - 1) : 0.0f;
            else
                factor = (width > 1) ? (float)x / (float)(width - 1) : 0.0f;

            image_pixel_write(&image,
                              x,
                              y,
                              graphic_color_lerp(start, end, factor));
        }
    }

    return image;
}

graphic_image_t graphic_image_generate_gradient_radial(int             width,
                                                       int             height,
                                                       float           density,
                                                       graphic_color_t inner,
                                                       graphic_color_t outer)
{
    graphic_image_t image = image_blank(width, height);

    if (image.data == NULL)
        return image;

    float center_x = (float)width / 2.0f;
    float center_y = (float)height / 2.0f;
    float radius   = (width < height) ? center_x : center_y;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx       = (float)x - center_x;
            float dy       = (float)y - center_y;
            float distance = sqrtf(dx * dx + dy * dy);
            float factor;

            /*
             * Density pushes the flat inner disc outwards. At
             * 1 there is no ramp left to divide by, and the
             * gradient becomes a hard edge.
             */
            if (density < 1.0f)
                factor = (distance -
                          radius * density) / (radius * (1.0f - density));
            else
                factor = (distance < radius) ? 0.0f : 1.0f;

            if (factor < 0.0f)
                factor = 0.0f;
            if (factor > 1.0f)
                factor = 1.0f;

            image_pixel_write(&image,
                              x,
                              y,
                              graphic_color_lerp(inner, outer, factor));
        }
    }

    return image;
}

graphic_image_t graphic_image_generate_gradient_square(int             width,
                                                       int             height,
                                                       float           density,
                                                       graphic_color_t inner,
                                                       graphic_color_t outer)
{
    graphic_image_t image = image_blank(width, height);

    if (image.data == NULL)
        return image;

    float center_x = (float)width / 2.0f;
    float center_y = (float)height / 2.0f;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            /*
             * Chebyshev distance instead of Euclidean, which
             * is what makes the falloff square.
             */
            float dx       = fabsf((float)x - center_x) / center_x;
            float dy       = fabsf((float)y - center_y) / center_y;
            float distance = (dx > dy) ? dx : dy;
            float factor;

            if (density < 1.0f)
                factor = (distance - density) / (1.0f - density);
            else
                factor = (distance < 1.0f) ? 0.0f : 1.0f;

            if (factor < 0.0f)
                factor = 0.0f;
            if (factor > 1.0f)
                factor = 1.0f;

            image_pixel_write(&image,
                              x,
                              y,
                              graphic_color_lerp(inner, outer, factor));
        }
    }

    return image;
}

graphic_image_t graphic_image_generate_checked(int             width,
                                               int             height,
                                               int             checks_x,
                                               int             checks_y,
                                               graphic_color_t first,
                                               graphic_color_t second)
{
    graphic_image_t image = image_blank(width, height);

    if (image.data == NULL || checks_x <= 0 || checks_y <= 0)
        return image;

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            image_pixel_write(&image,
                              x,
                              y,
                              ((x / checks_x + y / checks_y) % 2 == 0) ?
                              first :
                              second);

    return image;
}

/*
 * A deterministic hash rather than rand(), so the same call always
 * produces the same image, whatever else the program has been doing
 * to the global random state.
 */
static unsigned int noise_hash(unsigned int x,
                               unsigned int y,
                               unsigned int seed)
{
    unsigned int h = x * 374761393u + y * 668265263u + seed * 2147483647u;

    h = (h ^ (h >> 13)) * 1274126177u;

    return h ^ (h >> 16);
}

graphic_image_t graphic_image_generate_white_noise(int   width,
                                                   int   height,
                                                   float factor)
{
    graphic_image_t image = image_blank(width, height);
    graphic_color_t white = { 255, 255, 255, 255 };
    graphic_color_t black = {   0,   0,   0, 255 };
    int             x;
    int             y;

    if (image.data == NULL)
        return image;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            float value = (float)(noise_hash((unsigned int)x,
                                             (unsigned int)y,
                                             1u) % 100000u) / 100000.0f;

            image_pixel_write(&image, x, y, (value < factor) ? white : black);
        }
    }

    return image;
}

static float perlin_fade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float perlin_gradient(unsigned int hash, float x, float y)
{
    /* eight directions, picked by the low bits of the hash */
    switch (hash & 7u) {
        case 0:
            return x + y;
        case 1:
            return -x + y;
        case 2:
            return x - y;
        case 3:
            return -x - y;
        case 4:
            return x;
        case 5:
            return -x;
        case 6:
            return y;
        default:
            return -y;
    }
}

graphic_image_t graphic_image_generate_perlin_noise(int   width,
                                                    int   height,
                                                    int   offset_x,
                                                    int   offset_y,
                                                    float scale)
{
    graphic_image_t image = image_blank(width, height);

    if (image.data == NULL)
        return image;

    if (scale <= 0.0f)
        scale = 1.0f;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float           sample_x = (float)(x + offset_x) / scale;
            float           sample_y = (float)(y + offset_y) / scale;
            int             cell_x   = (int)floorf(sample_x);
            int             cell_y   = (int)floorf(sample_y);
            float           fx       = sample_x - (float)cell_x;
            float           fy       = sample_y - (float)cell_y;
            float           u        = perlin_fade(fx);
            float           v        = perlin_fade(fy);

            float           value;
            graphic_color_t color;
            uint8_t         level;

            float n00 = perlin_gradient(noise_hash((unsigned int)cell_x,
                                                   (unsigned int)cell_y,
                                                   7u),
                                        fx,
                                        fy);

            float n10 = perlin_gradient(noise_hash((unsigned int)cell_x + 1u,
                                                   (unsigned int)cell_y,
                                                   7u),
                                        fx - 1.0f,
                                        fy);

            float n01 = perlin_gradient(noise_hash((unsigned int)cell_x,
                                                   (unsigned int)cell_y + 1u,
                                                   7u),
                                        fx,
                                        fy - 1.0f);

            float n11 = perlin_gradient(noise_hash((unsigned int)cell_x + 1u,
                                                   (unsigned int)cell_y + 1u,
                                                   7u),
                                        fx - 1.0f,
                                        fy - 1.0f);

            value = (n00 + u * (n10 - n00)) + 
                v * ((n01 + u * (n11 - n01)) - (n00 + u * (n10 - n00)));

            /* map from about [-1, 1] into a grey level */
            value = (value + 1.0f) * 0.5f;
            if (value < 0.0f)
                value = 0.0f;
            if (value > 1.0f)
                value = 1.0f;

            level   = (uint8_t)(value * 255.0f);
            color.r = level;
            color.g = level;
            color.b = level;
            color.a = 255;

            image_pixel_write(&image, x, y, color);
        }
    }

    return image;
}

/* Worley noise: brightness is the distance to the nearest seed point */
graphic_image_t graphic_image_generate_cellular(int width,
                                                int height,
                                                int tile_size)
{
    graphic_image_t image = image_blank(width, height);

    if (image.data == NULL || tile_size <= 0)
        return image;

    int     seeds_x = width  / tile_size + 1;
    int     seeds_y = height / tile_size + 1;
    vec2_t *seeds   = (vec2_t*)malloc((size_t)(seeds_x * seeds_y) * sizeof(vec2_t));

    if (seeds == NULL)
        return image;

    for (int i = 0; i < seeds_x * seeds_y; i++) {
        int tile_x = i % seeds_x;
        int tile_y = i / seeds_x;
        unsigned int hash = noise_hash((unsigned int)tile_x,
                                       (unsigned int)tile_y,
                                       3u);

        seeds[i].x = (float)(tile_x *
                             tile_size + (int)(hash % (unsigned int)tile_size));
        seeds[i].y = (float)(tile_y * tile_size +
                             (int)((hash >> 8) % (unsigned int)tile_size));
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int             tile_x  = x / tile_size;
            int             tile_y  = y / tile_size;
            float           nearest = (float)(tile_size * 2);

            /*
             * A seed outside the nine neighbouring tiles is more
             * than a tile away, where the brightness has already
             * saturated, so searching only those nine is exact
             * and keeps the cost per pixel constant.
             */
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int    nx = tile_x + dx;
                    int    ny = tile_y + dy;
                    float  distance;
                    vec2_t seed;

                    if (nx < 0 || ny < 0 || nx >= seeds_x || ny >= seeds_y)
                        continue;

                    seed = seeds[ny * seeds_x + nx];
                    distance = sqrtf(((float)x - seed.x) * ((float)x - seed.x) +
                                     ((float)y - seed.y) * ((float)y - seed.y));

                    if (distance < nearest)
                        nearest = distance;
                }
            }

            /*
             * The nearest seed can be up to a tile diagonal
             * away, past what fits in a byte, and converting
             * an out of range float to an integer is undefined.
             */
            nearest = nearest * 255.0f / (float)tile_size;
            uint8_t level = (uint8_t)((nearest < 255.0f) ? nearest : 255.0f);

            graphic_color_t color;
            color.r = level;
            color.g = level;
            color.b = level;
            color.a = 255;

            image_pixel_write(&image, x, y, color);
        }
    }

    free(seeds);

    return image;
}

/* ------------------------------------------------------------------ */
/* queries                                                            */
/* ------------------------------------------------------------------ */

graphic_color_t graphic_image_color_get(graphic_image_t image, int x, int y)
{
    graphic_color_t clear = { 0, 0, 0, 0 };

    if (!image_usable(&image))
        return clear;

    return image_pixel_read(&image, x, y);
}

/*
 * The threshold is a fraction of full alpha. Clamped first, because
 * converting a float outside the byte range to uint8_t is undefined.
 */
static uint8_t alpha_limit_get(float threshold)
{
    if (threshold <= 0.0f)
        return 0;
    if (threshold >= 1.0f)
        return 255;

    return (uint8_t)(threshold * 255.0f);
}

/* the smallest rectangle holding every pixel above the alpha threshold */
rect_t graphic_image_alpha_border_get(graphic_image_t image, float threshold)
{
    rect_t  border = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (!image_usable(&image))
        return border;

    int     max_x = -1;
    int     max_y = -1;
    int     min_x = image.width;
    int     min_y = image.height;
    uint8_t limit = alpha_limit_get(threshold);

    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            if (image_pixel_read(&image, x, y).a <= limit)
                continue;

            if (x < min_x)
                min_x = x;
            if (y < min_y)
                min_y = y;
            if (x > max_x)
                max_x = x;
            if (y > max_y)
                max_y = y;
        }
    }

    if (max_x < min_x || max_y < min_y)
        return border; /* fully transparent */

    border.x      = (float)min_x;
    border.y      = (float)min_y;
    border.width  = (float)(max_x - min_x + 1);
    border.height = (float)(max_y - min_y + 1);

    return border;
}

graphic_color_t *graphic_image_colors_load(graphic_image_t image)
{
    graphic_color_t *colors;

    if (!image_usable(&image))
        return NULL;

    int count = image.width * image.height;

    colors = (graphic_color_t *)malloc((size_t)count * sizeof(graphic_color_t));
    if (colors == NULL)
        return NULL;

    for (int i = 0; i < count; i++)
        colors[i] = image_pixel_read(&image, i % image.width, i / image.width);

    return colors;
}

void graphic_image_colors_free(graphic_color_t *colors)
{
    free(colors);
}

/*
 * A linear scan against the palette collected so far. Quadratic in the
 * palette size, but the palette is capped at a few hundred entries and
 * this runs once, not per frame.
 */
graphic_color_t *graphic_image_palette_load(graphic_image_t image,
                                            int             max_palette_size,
                                            int            *color_count)
{
    graphic_color_t *palette;
    
    if (color_count != NULL)
        *color_count = 0;

    if (!image_usable(&image) || max_palette_size <= 0)
        return NULL;

    palette = (graphic_color_t *)calloc((size_t)max_palette_size,
                                        sizeof(graphic_color_t));
    if (palette == NULL)
        return NULL;

    int total = image.width * image.height;
    int count = 0;

    for (int i = 0; i < total && count < max_palette_size; i++) {
        graphic_color_t color = image_pixel_read(&image,
                                                 i % image.width,
                                                 i / image.width);

        if (color.a == 0)
            continue; /* fully transparent is not a colour */

        bool seen = false;

        for (int j = 0; j < count; j++) {
            if (graphic_color_is_equal(palette[j], color)) {
                seen = true;
                break;
            }
        }

        if (!seen)
            palette[count++] = color;
    }

    if (color_count != NULL)
        *color_count = count;

    return palette;
}

void graphic_image_palette_free(graphic_color_t *colors)
{
    free(colors);
}

/* ------------------------------------------------------------------ */
/* geometry operations                                                */
/* ------------------------------------------------------------------ */

/* the shared tail of every operation that replaces the pixel buffer */
static void image_replace(graphic_image_t *image, graphic_image_t built)
{
    if (built.data == NULL)
        return;

    free(image->data);
    *image = built;
}

void graphic_image_crop(graphic_image_t *image, rect_t crop)
{
    graphic_image_t result;

    if (!image_usable(image))
        return;

    int left   = (int)crop.x;
    int top    = (int)crop.y;
    int width  = (int)crop.width;
    int height = (int)crop.height;

    /*
     * Clamp rather than refuse: a crop hanging off the edge is a
     * routine result of arithmetic on a rectangle.
     */
    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    if (left + width > image->width)
        width = image->width - left;
    if (top + height > image->height)
        height = image->height - top;
    if (width <= 0 || height <= 0)
        return;

    result = image_blank(width, height);
    if (result.data == NULL)
        return;

    graphic_image_format(&result, image->format);

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            image_pixel_write(&result,
                              x,
                              y,
                              image_pixel_read(image, left + x, top + y));

    image_replace(image, result);
}

void graphic_image_alpha_crop(graphic_image_t *image, float threshold)
{
    rect_t border;

    if (!image_usable(image))
        return;

    border = graphic_image_alpha_border_get(*image, threshold);

    if (border.width > 0.0f && border.height > 0.0f)
        graphic_image_crop(image, border);
}

void graphic_image_alpha_clear(graphic_image_t *image,
                               graphic_color_t  color,
                               float            threshold)
{
    if (!image_usable(image))
        return;

    uint8_t limit = alpha_limit_get(threshold);

    for (int y = 0; y < image->height; y++)
        for (int x = 0; x < image->width; x++)
            if (image_pixel_read(image, x, y).a <= limit)
                image_pixel_write(image, x, y, color);
}

void graphic_image_alpha_mask(graphic_image_t *image,
                              graphic_image_t  alpha_mask)
{
    if (!image_usable(image) || !image_usable(&alpha_mask))
        return;

    if (image->width != alpha_mask.width || image->height != alpha_mask.height)
        return; /* sizes must match, no scaling here */

    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            graphic_color_t color = image_pixel_read(image, x, y);
            graphic_color_t mask  = image_pixel_read(&alpha_mask, x, y);

            /*
             * The mask is read as grey, so its red channel is
             * as good as its luminance.
             */
            color.a = mask.r;
            image_pixel_write(image, x, y, color);
        }
    }
}

void graphic_image_alpha_premultiply(graphic_image_t *image)
{
    if (!image_usable(image))
        return;

    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            graphic_color_t color = image_pixel_read(image, x, y);
            float           alpha = (float)color.a / 255.0f;

            color.r = (uint8_t)((float)color.r * alpha);
            color.g = (uint8_t)((float)color.g * alpha);
            color.b = (uint8_t)((float)color.b * alpha);

            image_pixel_write(image, x, y, color);
        }
    }
}

void graphic_image_resize(graphic_image_t *image,
                          int              new_width,
                          int              new_height)
{
    graphic_image_t result;

    if (!image_usable(image) || new_width <= 0 || new_height <= 0)
        return;

    result = image_blank(new_width, new_height);
    if (result.data == NULL)
        return;

    graphic_image_format(&result, image->format);

    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            /*
             * Sample at pixel centres, so the edges do not
             * drift by half a texel when scaling up.
             */
            float sx = ((float)x +
                        0.5f) * (float)image->width / (float)new_width - 0.5f;
            float sy = ((float)y +
                        0.5f) * (float)image->height / (float)new_height - 0.5f;

            int x0 = (int)floorf(sx);
            if (x0 < 0)
                x0 = 0;

            int y0 = (int)floorf(sy);
            if (y0 < 0)
                y0 = 0;

            int x1 = x0 + 1;
            if (x1 > image->width - 1)
                x1 = image->width - 1;

            int y1 = y0 + 1;
            if (y1 > image->height - 1)
                y1 = image->height - 1;

            graphic_color_t c00 = image_pixel_read(image, x0, y0);
            graphic_color_t c10 = image_pixel_read(image, x1, y0);
            graphic_color_t c01 = image_pixel_read(image, x0, y1);
            graphic_color_t c11 = image_pixel_read(image, x1, y1);

            float fx = sx - (float)x0;
            float fy = sy - (float)y0;

            graphic_color_t top    = graphic_color_lerp(c00, c10, fx);
            graphic_color_t bottom = graphic_color_lerp(c01, c11, fx);

            image_pixel_write(&result,
                              x,
                              y,
                              graphic_color_lerp(top, bottom, fy));
        }
    }

    image_replace(image, result);
}

void graphic_image_resize_nn(graphic_image_t *image,
                             int              new_width,
                             int              new_height)
{
    graphic_image_t result;

    if (!image_usable(image) || new_width <= 0 || new_height <= 0)
        return;

    result = image_blank(new_width, new_height);
    if (result.data == NULL)
        return;

    graphic_image_format(&result, image->format);

    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            int sx = x * image->width / new_width;
            int sy = y * image->height / new_height;

            image_pixel_write(&result, x, y, image_pixel_read(image, sx, sy));
        }
    }

    image_replace(image, result);
}

void graphic_image_resize_canvas(graphic_image_t *image,
                                 int              new_width,
                                 int              new_height,
                                 int              offset_x,
                                 int              offset_y,
                                 graphic_color_t  fill)
{
    graphic_image_t result;

    if (!image_usable(image) || new_width <= 0 || new_height <= 0)
        return;

    result = image_blank(new_width, new_height);
    if (result.data == NULL)
        return;

    graphic_image_format(&result, image->format);

    for (int y = 0; y < new_height; y++)
        for (int x = 0; x < new_width; x++)
            image_pixel_write(&result, x, y, fill);

    for (int y = 0; y < image->height; y++)
        for (int x = 0; x < image->width; x++)
            image_pixel_write(&result,
                              x + offset_x,
                              y + offset_y,
                              image_pixel_read(image, x, y));

    image_replace(image, result);
}

static int next_power_of_two(int value)
{
    int result = 1;

    while (result < value)
        result *= 2;

    return result;
}

void graphic_image_to_pot(graphic_image_t *image, graphic_color_t fill)
{
    if (!image_usable(image))
        return;

    int width  = next_power_of_two(image->width);
    int height = next_power_of_two(image->height);

    if (width == image->width && height == image->height)
        return;

    graphic_image_resize_canvas(image, width, height, 0, 0, fill);
}

void graphic_image_flip_vertical(graphic_image_t *image)
{
    graphic_image_t result;

    if (!image_usable(image))
        return;

    result = image_blank(image->width, image->height);
    if (result.data == NULL)
        return;

    graphic_image_format(&result, image->format);

    for (int y = 0; y < image->height; y++)
        for (int x = 0; x < image->width; x++)
            image_pixel_write(&result,
                              x,
                              image->height - 1 - y,
                              image_pixel_read(image, x, y));

    image_replace(image, result);
}

void graphic_image_flip_horizontal(graphic_image_t *image)
{
    graphic_image_t result;

    if (!image_usable(image))
        return;

    result = image_blank(image->width, image->height);
    if (result.data == NULL)
        return;

    graphic_image_format(&result, image->format);

    for (int y = 0; y < image->height; y++)
        for (int x = 0; x < image->width; x++)
            image_pixel_write(&result,
                              image->width - 1 - x,
                              y,
                              image_pixel_read(image, x, y));

    image_replace(image, result);
}

/*
 * Rotation by an arbitrary angle keeps the original size, so corners
 * are lost and the edges gain transparent pixels. The quarter turns
 * below are separate because they are exact and change the size.
 */
void graphic_image_rotate(graphic_image_t *image, int degrees)
{
    graphic_image_t result;

    if (!image_usable(image))
        return;

    degrees = ((degrees % 360) + 360) % 360;

    if (degrees == 0)
        return;
    if (degrees == 90) {
        graphic_image_rotate_cw(image);
        return;
    }
    if (degrees == 180) {
        graphic_image_flip_horizontal(image);
        graphic_image_flip_vertical(image);
        return;
    }
    if (degrees == 270) {
        graphic_image_rotate_ccw(image);
        return;
    }

    result = image_blank(image->width, image->height);
    if (result.data == NULL)
        return;

    graphic_image_format(&result, image->format);

    float radians   = (float)degrees * DEG2RAD;
    float sin_angle = sinf(radians);
    float cos_angle = cosf(radians);
    float center_x  = (float)image->width / 2.0f;
    float center_y  = (float)image->height / 2.0f;

    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            /*
             * Sampled backwards from the destination, which
             * is what keeps the result free of holes. Pixel
             * centres sit at half integers, and floorf()
             * rather than a cast: truncation towards zero
             * pulled samples from just outside the left and
             * top edges back onto them, so only those two
             * edges came out smeared.
             */
            float dx = (float)x + 0.5f - center_x;
            float dy = (float)y + 0.5f - center_y;
            int   sx = (int)floorf(dx * cos_angle + dy * sin_angle + center_x);
            int   sy = (int)floorf(-dx * sin_angle + dy * cos_angle + center_y);

            if (sx < 0 || sy < 0 || sx >= image->width || sy >= image->height)
                continue;

            image_pixel_write(&result, x, y, image_pixel_read(image, sx, sy));
        }
    }

    image_replace(image, result);
}

void graphic_image_rotate_cw(graphic_image_t *image)
{
    graphic_image_t result;

    if (!image_usable(image))
        return;

    result = image_blank(image->height, image->width);
    if (result.data == NULL)
        return;

    graphic_image_format(&result, image->format);

    for (int y = 0; y < image->height; y++)
        for (int x = 0; x < image->width; x++)
            image_pixel_write(&result,
                              image->height - 1 - y,
                              x,
                              image_pixel_read(image, x, y));

    image_replace(image, result);
}

void graphic_image_rotate_ccw(graphic_image_t *image)
{
    graphic_image_t result;

    if (!image_usable(image))
        return;

    result = image_blank(image->height, image->width);
    if (result.data == NULL)
        return;

    graphic_image_format(&result, image->format);

    for (int y = 0; y < image->height; y++)
        for (int x = 0; x < image->width; x++)
            image_pixel_write(&result,
                              y,
                              image->width - 1 - x,
                              image_pixel_read(image, x, y));

    image_replace(image, result);
}

/* ------------------------------------------------------------------ */
/* drawing into an image                                              */
/* ------------------------------------------------------------------ */

void graphic_image_clear_background(graphic_image_t *destination,
                                    graphic_color_t  color)
{
    if (!image_usable(destination))
        return;

    for (int y = 0; y < destination->height; y++)
        for (int x = 0; x < destination->width; x++)
            image_pixel_write(destination, x, y, color);
}

/*
 * Writes rather than blends. Only graphic_image_draw composites,
 * because that is the one call where a source alpha is meaningful.
 */
void graphic_image_draw_pixel(graphic_image_t *destination,
                              int              x,
                              int              y,
                              graphic_color_t  color)
{
    if (!image_usable(destination))
        return;

    image_pixel_write(destination, x, y, color);
}

void graphic_image_draw_pixel_v(graphic_image_t *destination,
                                vec2_t           position,
                                graphic_color_t  color)
{
    graphic_image_draw_pixel(destination,
                             (int)position.x,
                             (int)position.y,
                             color);
}

void graphic_image_draw_rectangle_rec(graphic_image_t *destination,
                                      rect_t           rectangle,
                                      graphic_color_t  color)
{
    if (!image_usable(destination))
        return;

    int left   = (int)rectangle.x;
    int top    = (int)rectangle.y;
    int right  = left + (int)rectangle.width;
    int bottom = top + (int)rectangle.height;

    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    if (right > destination->width)
        right = destination->width;
    if (bottom > destination->height)
        bottom = destination->height;

    for (int y = top; y < bottom; y++)
        for (int x = left; x < right; x++)
            image_pixel_write(destination, x, y, color);
}

void graphic_image_draw_rectangle(graphic_image_t *destination,
                                  int              x,
                                  int              y,
                                  int              width,
                                  int              height,
                                  graphic_color_t  color)
{
    rect_t rectangle = { (float)x, (float)y, (float)width, (float)height };

    graphic_image_draw_rectangle_rec(destination, rectangle, color);
}

void graphic_image_draw_rectangle_v(graphic_image_t *destination,
                                    vec2_t           position,
                                    vec2_t           size,
                                    graphic_color_t  color)
{
    rect_t rectangle = { position.x, position.y, size.x, size.y };

    graphic_image_draw_rectangle_rec(destination, rectangle, color);
}

/* ------------------------------------------------------------------ */
/* file formats                                                       */
/*                                                                    */
/* BMP is handled here because an uncompressed 32 bit BMP is a header */
/* and a pixel array. Everything else needs a decoder, which is what  */
/* GRAPHIC_USE_STB_IMAGE brings in.                                   */
/* ------------------------------------------------------------------ */

#define BMP_HEADER_SIZE    54

static void write_le16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value & 0xff);
    out[1] = (uint8_t)((value >> 8) & 0xff);
}

static void write_le32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xff);
    out[1] = (uint8_t)((value >> 8) & 0xff);
    out[2] = (uint8_t)((value >> 16) & 0xff);
    out[3] = (uint8_t)((value >> 24) & 0xff);
}

static uint32_t read_le32(const uint8_t *in)
{
    return (uint32_t)in[0]        |
          ((uint32_t)in[1] << 8)  |
          ((uint32_t)in[2] << 16) |
          ((uint32_t)in[3] << 24);
}

static uint16_t read_le16(const uint8_t *in)
{
    return (uint16_t)((uint32_t)in[0] | ((uint32_t)in[1] << 8));
}

/* 32 bit BGRA, bottom up, which every reader accepts */
static unsigned char *bmp_encode(graphic_image_t image, int *size)
{
    int pixel_bytes = image.width * image.height * 4;
    int total       = BMP_HEADER_SIZE + pixel_bytes;

    uint8_t *out = (uint8_t *)calloc(1, (size_t)total);
    if (out == NULL)
        return NULL;

    out[0] = 'B';
    out[1] = 'M';
    write_le32(out + 2, (uint32_t)total);
    write_le32(out + 10, BMP_HEADER_SIZE);

    write_le32(out + 14, 40); /* header size */
    write_le32(out + 18, (uint32_t)image.width);
    write_le32(out + 22, (uint32_t)image.height);
    write_le16(out + 26, 1);  /* planes */
    write_le16(out + 28, 32); /* bits per pixel */
    write_le32(out + 34, (uint32_t)pixel_bytes);

    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            graphic_color_t color = image_pixel_read(&image, x, y);
            /* rows run bottom to top in a BMP */
            size_t offset = (size_t)BMP_HEADER_SIZE +
                            ((size_t)(image.height - 1 -
                                      y) * (size_t)image.width + (size_t)x) * 4;

            out[offset + 0] = color.b;
            out[offset + 1] = color.g;
            out[offset + 2] = color.r;
            out[offset + 3] = color.a;
        }
    }

    if (size != NULL)
        *size = total;

    return out;
}

static graphic_image_t bmp_decode(const unsigned char *data, int size)
{
    graphic_image_t image = { NULL, 0, 0, 0, 0 };

    if (data == NULL || size < BMP_HEADER_SIZE)
        return image;

    if (data[0] != 'B' || data[1] != 'M')
        return image;

    uint32_t offset    = read_le32(data + 10);
    int32_t  width     = (int32_t)read_le32(data + 18);
    int32_t  height    = (int32_t)read_le32(data + 22);
    uint16_t bits      = read_le16(data + 28);
    bool     bottom_up = true;

    if (height < 0) {
        height    = -height; /* a negative height means top down */
        bottom_up = false;
    }

    if (width <= 0 || height <= 0 || (bits != 24 && bits != 32))
        return image;

    {
        int bytes_per_pixel = bits / 8;
        /*
         * Rows are padded to a multiple of four bytes. The header
         * comes from a file, so the bounds are checked in 64 bits
         * and by division: a forged width or height must not
         * overflow its way past the check.
         */
        int64_t stride    = ((int64_t)width * bytes_per_pixel + 3) / 4 * 4;
        bool    any_alpha = false;

        if (offset > (uint32_t)size ||
            stride > ((int64_t)size - offset) / height)
            return image;

        image = image_blank(width, height);
        if (image.data == NULL)
            return image;

        for (int y = 0; y < height; y++) {
            int            row  = bottom_up ? (height - 1 - y) : y;
            const uint8_t *line = data + offset + (size_t)row * (size_t)stride;

            for (int x = 0; x < width; x++) {
                const uint8_t *pixel = line +
                                       (size_t)x * (size_t)bytes_per_pixel;

                graphic_color_t color;
                color.b   = pixel[0];
                color.g   = pixel[1];
                color.r   = pixel[2];
                color.a   = (bits == 32) ? pixel[3] : 255;
                any_alpha = any_alpha || color.a != 0;

                image_pixel_write(&image, x, y, color);
            }
        }

        /*
         * In a plain 32 bit BMP the fourth byte is officially
         * unused, and most writers leave it zero. Taken at face
         * value, such a file would load fully transparent, so
         * an alpha channel that is zero everywhere means opaque,
         * the same rule stb_image applies.
         */
        if (!any_alpha) {
            uint8_t *bytes = (uint8_t *)image.data;
            size_t   count = (size_t)width * (size_t)height;

            for (size_t i = 0; i < count; i++)
                bytes[i * 4 + 3] = 255;
        }
    }

    return image;
}

static bool extension_matches(const char *file_type, const char *wanted)
{
    const char *cursor = file_type;

    if (file_type == NULL)
        return false;

    if (*cursor == '.')
        cursor++;

    while (*cursor != '\0' && *wanted != '\0') {
        char a = *cursor;
        char b = *wanted;

        if (a >= 'A' && a <= 'Z')
            a = (char)(a + 32);

        if (a != b)
            return false;

        cursor++;
        wanted++;
    }

    return *cursor == '\0' && *wanted == '\0';
}

graphic_image_t graphic_image_load_from_memory(const char          *file_type,
                                               const unsigned char *file_data,
                                               int                  data_size)
{
    graphic_image_t image = { NULL, 0, 0, 0, 0 };

    if (file_data == NULL || data_size <= 0)
        return image;

    if (extension_matches(file_type, "bmp"))
        return bmp_decode(file_data, data_size);

#ifdef GRAPHIC_USE_STB_IMAGE
    {
        int            width    = 0;
        int            height   = 0;
        int            channels = 0;
        unsigned char *pixels;

        pixels = stbi_load_from_memory(file_data,
                                       data_size,
                                       &width,
                                       &height,
                                       &channels,
                                       4);
        if (pixels == NULL)
            return image;

        image.data    = pixels;
        image.width   = width;
        image.height  = height;
        image.mipmaps = 1;
        image.format  = GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8A8;
    }
#else
    (void)file_type; /* built without a decoder */
#endif

    return image;
}

graphic_image_t graphic_image_load(const char *file_name)
{
    graphic_image_t image = { NULL, 0, 0, 0, 0 };

    if (file_name == NULL)
        return image;

    FILE *file = fopen(file_name, "rb");
    if (file == NULL)
        return image;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0) {
        fclose(file);
        return image;
    }

    unsigned char  *data = (unsigned char *)malloc((size_t)size);
    if (data == NULL) {
        fclose(file);
        return image;
    }

    if (fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return image;
    }

    fclose(file);

    const char *extension = strrchr(file_name, '.');
    image     = graphic_image_load_from_memory(extension, data, (int)size);

    free(data);

    return image;
}

/*
 * No header, no metadata: the caller states the geometry and the
 * format, and header_size skips whatever sits in front of the pixels.
 */
graphic_image_t graphic_image_load_raw(const char *file_name,
                                       int         width,
                                       int         height,
                                       int         format,
                                       int         header_size)
{
    graphic_image_t image = { NULL, 0, 0, 0, 0 };

    if (file_name == NULL || width <= 0 || height <= 0)
        return image;

    int size = graphic_pixel_data_size_get(width, height, format);
    if (size <= 0)
        return image;

    FILE *file = fopen(file_name, "rb");
    if (file == NULL)
        return image;

    if (header_size > 0)
        fseek(file, header_size, SEEK_SET);

    image.data = malloc((size_t)size);
    if (image.data == NULL) {
        fclose(file);
        return image;
    }

    if (fread(image.data, 1, (size_t)size, file) != (size_t)size) {
        free(image.data);
        image.data = NULL;
        fclose(file);
        return image;
    }

    fclose(file);

    image.width   = width;
    image.height  = height;
    image.mipmaps = 1;
    image.format  = format;

    return image;
}

unsigned char *graphic_image_export_to_memory(graphic_image_t image,
                                              const char     *file_type,
                                              int            *file_size)
{
    if (file_size != NULL)
        *file_size = 0;

    if (!image_usable(&image))
        return NULL;

    if (extension_matches(file_type, "bmp"))
        return bmp_encode(image, file_size);

#ifdef GRAPHIC_USE_STB_IMAGE_WRITE
    if (extension_matches(file_type, "png")) {
        graphic_image_t rgba   = graphic_image_copy(image);
        int             length = 0;
        unsigned char  *out;

        graphic_image_format(&rgba, GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8A8);
        out = stbi_write_png_to_mem((const unsigned char *)rgba.data,
                                    rgba.width * 4,
                                    rgba.width,
                                    rgba.height,
                                    4,
                                    &length);
        graphic_image_free(rgba);

        if (file_size != NULL)
            *file_size = length;

        return out;
    }
#endif

    return NULL;
}

bool graphic_image_export(graphic_image_t image, const char *file_name)
{
    if (file_name == NULL || !image_usable(&image))
        return false;

    int            size      = 0;
    const char    *extension = strrchr(file_name, '.');
    unsigned char *data      = graphic_image_export_to_memory(image,
                                                              extension,
                                                              &size);

    if (data == NULL || size <= 0) {
        free(data);
        return false;
    }

    FILE *file = fopen(file_name, "wb");
    if (file == NULL) {
        free(data);
        return false;
    }

    bool written = fwrite(data, 1, (size_t)size, file) == (size_t)size;

    fclose(file);
    free(data);

    return written;
}

/* the same trick the shaders use: pixels as a C array, no file at run time */
bool graphic_image_export_as_code(graphic_image_t image,
                                  const char     *file_name)
{
    if (file_name == NULL || !image_usable(&image))
        return false;

    graphic_image_t rgba = graphic_image_copy(image);
    graphic_image_format(&rgba, GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8A8);

    if (rgba.format != GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8A8) {
        graphic_image_free(rgba);
        return false; /* out of memory */
    }

    FILE *file = fopen(file_name, "w");
    if (file == NULL) {
        graphic_image_free(rgba);
        return false;
    }

    const unsigned char *bytes;
    int                  size;

    size  = graphic_pixel_data_size_get(rgba.width, rgba.height, rgba.format);
    bytes = (const unsigned char *)rgba.data;

    fprintf(file, "/* generated by graphic_image_export_as_code */\n\n");
    fprintf(file, "#define IMAGE_WIDTH  %d\n", rgba.width);
    fprintf(file, "#define IMAGE_HEIGHT %d\n", rgba.height);
    fprintf(file, "#define IMAGE_FORMAT %d\n\n", rgba.format);
    fprintf(file, "static unsigned char image_data[%d] = {\n", size);

    for (int i = 0; i < size; i++) {
        if (i % 16 == 0)
            fprintf(file, "\t");

        fprintf(file, "0x%02x%s", bytes[i], (i == size - 1) ? "" : ", ");

        if (i % 16 == 15 || i == size - 1)
            fprintf(file, "\n");
    }

    fprintf(file, "};\n");

    fclose(file);
    graphic_image_free(rgba);

    return true;
}