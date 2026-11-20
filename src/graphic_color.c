#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "graphic_color.h"

static uint8_t clamp_to_byte(float value)
{
    if (value < 0.0f)
        return 0;

    if (value > 255.0f)
        return 255;

    return (uint8_t)(value + 0.5f);
}

static float clamp_unit(float value)
{
    if (value < 0.0f)
        return 0.0f;

    if (value > 1.0f)
        return 1.0f;

    return value;
}

/* ------------------------------------------------------------------ */
/* half floats                                                        */
/*                                                                    */
/* IEEE 754 binary16, needed only by the R16 family. Denormals are    */
/* flushed to zero: the formats exist for image data, not arithmetic. */
/* ------------------------------------------------------------------ */

static float half_to_float(uint16_t half)
{
    uint32_t sign     = (uint32_t)(half >> 15) << 31;
    uint32_t exponent = (half >> 10) & 0x1f;
    uint32_t mantissa = half & 0x3ff;
    uint32_t bits;
    float    result;

    if (exponent == 0) {
        bits = sign; /* zero or denormal */
    } else if (exponent == 31) {
        bits = sign | 0x7f800000u | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
    }

    memcpy(&result, &bits, sizeof(result));

    return result;
}

static uint16_t float_to_half(float value)
{
    uint32_t bits;
    uint32_t sign;
    int32_t  exponent;
    uint32_t mantissa;

    memcpy(&bits, &value, sizeof(bits));

    sign     = (bits >> 16) & 0x8000u;
    exponent = (int32_t)((bits >> 23) & 0xff) - 112;
    mantissa = bits & 0x7fffffu;

    if (exponent <= 0)
        return (uint16_t)sign;
    if (exponent >= 31)
        return (uint16_t)(sign | 0x7c00u);

    return (uint16_t)(sign | ((uint32_t)exponent << 10) | (mantissa >> 13));
}

/* ------------------------------------------------------------------ */
/* colour operations                                                  */
/* ------------------------------------------------------------------ */

bool graphic_color_is_equal(graphic_color_t a, graphic_color_t b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

graphic_color_t graphic_color_fade(graphic_color_t color, float alpha)
{
    color.a = clamp_to_byte(clamp_unit(alpha) * 255.0f);

    return color;
}

graphic_color_t graphic_color_alpha(graphic_color_t color, float alpha)
{
    return graphic_color_fade(color, alpha);
}

int graphic_color_to_int(graphic_color_t color)
{
    return (int)(((unsigned int)color.r << 24) | ((unsigned int)color.g << 16) |
                 ((unsigned int)color.b << 8)  |  (unsigned int)color.a);
}

graphic_color_t graphic_color_get(unsigned int hex_value)
{
    graphic_color_t color;

    color.r = (uint8_t)((hex_value >> 24) & 0xff);
    color.g = (uint8_t)((hex_value >> 16) & 0xff);
    color.b = (uint8_t)((hex_value >> 8) & 0xff);
    color.a = (uint8_t)(hex_value & 0xff);

    return color;
}

vec4_t graphic_color_normalize(graphic_color_t color)
{
    vec4_t result;

    result.x = (float)color.r / 255.0f;
    result.y = (float)color.g / 255.0f;
    result.z = (float)color.b / 255.0f;
    result.w = (float)color.a / 255.0f;

    return result;
}

graphic_color_t graphic_color_from_normalized(vec4_t normalized)
{
    graphic_color_t color;

    color.r = clamp_to_byte(clamp_unit(normalized.x) * 255.0f);
    color.g = clamp_to_byte(clamp_unit(normalized.y) * 255.0f);
    color.b = clamp_to_byte(clamp_unit(normalized.z) * 255.0f);
    color.a = clamp_to_byte(clamp_unit(normalized.w) * 255.0f);

    return color;
}

/* hue in degrees, saturation and value in [0, 1] */
vec3_t graphic_color_to_hsv(graphic_color_t color)
{
    vec3_t hsv = { 0.0f, 0.0f, 0.0f };
    float  r   = (float)color.r / 255.0f;
    float  g   = (float)color.g / 255.0f;
    float  b   = (float)color.b / 255.0f;
    float  min = r;
    float  max = r;
    float  delta;

    if (g < min)
        min = g;

    if (b < min)
        min = b;

    if (g > max)
        max = g;

    if (b > max)
        max = b;

    hsv.z = max;
    delta = max - min;

    if (delta < MATH_EPSILON)
        return hsv; /* grey: hue is undefined, use 0 */

    hsv.y = (max > 0.0f) ? delta / max : 0.0f;

    if (max == r)
        hsv.x = (g - b) / delta;
    else if (max == g)
        hsv.x = 2.0f + (b - r) / delta;
    else
        hsv.x = 4.0f + (r - g) / delta;

    hsv.x *= 60.0f;

    if (hsv.x < 0.0f)
        hsv.x += 360.0f;

    return hsv;
}

graphic_color_t graphic_color_from_hsv(float hue,
                                       float saturation,
                                       float value)
{
    float rgb[3];

    /* the branchless formulation: one expression per channel */
    for (int channel = 0; channel < 3; channel++) {
        float k = fmodf((5.0f - (float)channel * 2.0f) + hue / 60.0f, 6.0f);
        float t = 4.0f - k;

        if (t > k)
            t = k;

        if (t > 1.0f)
            t = 1.0f;

        if (t < 0.0f)
            t = 0.0f;

        rgb[channel] = value - value * saturation * t;
    }

    graphic_color_t color = { 0, 0, 0, 255 };
    color.r = clamp_to_byte(rgb[0] * 255.0f);
    color.g = clamp_to_byte(rgb[1] * 255.0f);
    color.b = clamp_to_byte(rgb[2] * 255.0f);

    return color;
}

graphic_color_t graphic_color_tint(graphic_color_t color,
                                   graphic_color_t tint)
{
    graphic_color_t result;

    result.r = (uint8_t)(((int)color.r * (int)tint.r) / 255);
    result.g = (uint8_t)(((int)color.g * (int)tint.g) / 255);
    result.b = (uint8_t)(((int)color.b * (int)tint.b) / 255);
    result.a = (uint8_t)(((int)color.a * (int)tint.a) / 255);

    return result;
}

/* factor in [-1, 1]: negative darkens towards black, positive towards white */
graphic_color_t graphic_color_brightness(graphic_color_t color, float factor)
{
    float red   = (float)color.r;
    float green = (float)color.g;
    float blue  = (float)color.b;

    if (factor > 1.0f)
        factor = 1.0f;

    if (factor < -1.0f)
        factor = -1.0f;

    if (factor < 0.0f) {
        factor = 1.0f + factor;
        red   *= factor;
        green *= factor;
        blue  *= factor;
    } else {
        red   = (255.0f - red) * factor + red;
        green = (255.0f - green) * factor + green;
        blue  = (255.0f - blue) * factor + blue;
    }

    color.r = clamp_to_byte(red);
    color.g = clamp_to_byte(green);
    color.b = clamp_to_byte(blue);

    return color;
}

/* contrast in [-1, 1], applied about the midpoint */
graphic_color_t graphic_color_contrast(graphic_color_t color, float contrast)
{
    if (contrast < -1.0f)
        contrast = -1.0f;

    if (contrast > 1.0f)
        contrast = 1.0f;

    contrast = (1.0f + contrast) * (1.0f + contrast);

    float channel[3] = {
        (float)color.r / 255.0f,
        (float)color.g / 255.0f,
        (float)color.b / 255.0f
    };

    for (int i = 0; i < 3; i++) {
        float value = channel[i] - 0.5f;
        value      *= contrast;
        value      += 0.5f;
        channel[i]  = clamp_unit(value) * 255.0f;
    }

    color.r = clamp_to_byte(channel[0]);
    color.g = clamp_to_byte(channel[1]);
    color.b = clamp_to_byte(channel[2]);

    return color;
}

/*
 * Porter-Duff source over destination, with the result divided by its
 * own alpha, as raylib's ColorAlphaBlend() does. Over an opaque
 * destination that is exactly what the alpha blend pipeline writes;
 * over a translucent one the pipeline, which never divides, differs.
 */
graphic_color_t graphic_color_alpha_blend(graphic_color_t destination,
                                          graphic_color_t source,
                                          graphic_color_t tint)
{
    graphic_color_t result;

    source = graphic_color_tint(source, tint);

    if (source.a == 255)
        return source;
    if (source.a == 0)
        return destination;

    float source_alpha      = (float)source.a / 255.0f;
    float destination_alpha = (float)destination.a / 255.0f;
    float out_alpha = source_alpha + destination_alpha * (1.0f - source_alpha);

    if (out_alpha < MATH_EPSILON) {
        result.r = 0;
        result.g = 0;
        result.b = 0;
        result.a = 0;

        return result;
    }

    float src[3] = {
        (float)source.r / 255.0f,
        (float)source.g / 255.0f,
        (float)source.b / 255.0f
    };

    float dst[3] = {
        (float)destination.r / 255.0f,
        (float)destination.g / 255.0f,
        (float)destination.b / 255.0f
    };

    for (int i = 0; i < 3; i++) {
        float value = (src[i] * source_alpha +
                       dst[i] * destination_alpha *
                       (1.0f - source_alpha)) / out_alpha;

        src[i] = clamp_unit(value) * 255.0f;
    }

    result.r = clamp_to_byte(src[0]);
    result.g = clamp_to_byte(src[1]);
    result.b = clamp_to_byte(src[2]);
    result.a = clamp_to_byte(out_alpha * 255.0f);

    return result;
}

graphic_color_t graphic_color_lerp(graphic_color_t a,
                                   graphic_color_t b,
                                   float           factor)
{
    graphic_color_t result;

    factor = clamp_unit(factor);

    result.r = clamp_to_byte((float)a.r + ((float)b.r - (float)a.r) * factor);
    result.g = clamp_to_byte((float)a.g + ((float)b.g - (float)a.g) * factor);
    result.b = clamp_to_byte((float)a.b + ((float)b.b - (float)a.b) * factor);
    result.a = clamp_to_byte((float)a.a + ((float)b.a - (float)a.a) * factor);

    return result;
}

/* ------------------------------------------------------------------ */
/* raw pixels                                                         */
/* ------------------------------------------------------------------ */

int graphic_pixel_data_size_get(int width, int height, int format)
{
    int bytes_per_pixel;

    if (width <= 0 || height <= 0)
        return 0;

    switch (format) {
        case GRAPHIC_PIXEL_UNCOMPRESSED_GRAYSCALE:
            bytes_per_pixel = 1;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_GRAY_ALPHA:
        case GRAPHIC_PIXEL_UNCOMPRESSED_R5G6B5:
        case GRAPHIC_PIXEL_UNCOMPRESSED_R5G5B5A1:
        case GRAPHIC_PIXEL_UNCOMPRESSED_R4G4B4A4:
        case GRAPHIC_PIXEL_UNCOMPRESSED_R16:
            bytes_per_pixel = 2;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8:
            bytes_per_pixel = 3;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8A8:
        case GRAPHIC_PIXEL_UNCOMPRESSED_R32:
            bytes_per_pixel = 4;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R16G16B16:
            bytes_per_pixel = 6;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R16G16B16A16:
            bytes_per_pixel = 8;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R32G32B32:
            bytes_per_pixel = 12;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R32G32B32A32:
            bytes_per_pixel = 16;
            break;
        default:
            return 0; /* compressed, or not a format */
    }

    /*
     * Bytes rather than bits, and the product checked before it is
     * formed: width * height * bits overflowed an int already at
     * 8192 x 8192 RGBA, an ordinary texture size. Zero means the
     * image does not fit, which every caller treats as failure.
     */
    if (width > INT_MAX / bytes_per_pixel / height)
        return 0;

    return width * height * bytes_per_pixel;
}

graphic_color_t graphic_pixel_color_get(const void *source, int format)
{
    graphic_color_t color  = { 0, 0, 0, 255 };
    const uint8_t  *bytes  = (const uint8_t *)source;
    const uint16_t *shorts = (const uint16_t *)source;
    const float    *floats = (const float *)source;

    if (source == NULL)
        return color;

    switch (format) {
        case GRAPHIC_PIXEL_UNCOMPRESSED_GRAYSCALE:
            color.r = bytes[0];
            color.g = bytes[0];
            color.b = bytes[0];
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_GRAY_ALPHA:
            color.r = bytes[0];
            color.g = bytes[0];
            color.b = bytes[0];
            color.a = bytes[1];
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R5G6B5:
            /*
             * 5 and 6 bit channels are scaled, not shifted, so that
             * the maximum value maps to 255 exactly.
             */
            color.r = (uint8_t)((float)((shorts[0] >> 11) & 0x1f) * 255.0f / 31.0f);
            color.g = (uint8_t)((float)((shorts[0] >>  5) & 0x3f) * 255.0f / 63.0f);
            color.b = (uint8_t)((float)(shorts[0] & 0x1f) * 255.0f / 31.0f);
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R5G5B5A1:
            color.r = (uint8_t)((float)((shorts[0] >> 11) & 0x1f) * 255.0f / 31.0f);
            color.g = (uint8_t)((float)((shorts[0] >>  6) & 0x1f) * 255.0f / 31.0f);
            color.b = (uint8_t)((float)((shorts[0] >>  1) & 0x1f) * 255.0f / 31.0f);
            color.a = (shorts[0] & 0x1) ? 255 : 0;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R4G4B4A4:
            color.r = (uint8_t)((float)((shorts[0] >> 12) & 0xf) * 255.0f / 15.0f);
            color.g = (uint8_t)((float)((shorts[0] >>  8) & 0xf) * 255.0f / 15.0f);
            color.b = (uint8_t)((float)((shorts[0] >>  4) & 0xf) * 255.0f / 15.0f);
            color.a = (uint8_t)((float)(shorts[0] & 0xf) * 255.0f / 15.0f);
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8:
            color.r = bytes[0];
            color.g = bytes[1];
            color.b = bytes[2];
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8A8:
            color.r = bytes[0];
            color.g = bytes[1];
            color.b = bytes[2];
            color.a = bytes[3];
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R32:
            color.r = clamp_to_byte(clamp_unit(floats[0]) * 255.0f);
            color.g = color.r;
            color.b = color.r;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R32G32B32:
            color.r = clamp_to_byte(clamp_unit(floats[0]) * 255.0f);
            color.g = clamp_to_byte(clamp_unit(floats[1]) * 255.0f);
            color.b = clamp_to_byte(clamp_unit(floats[2]) * 255.0f);
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R32G32B32A32:
            color.r = clamp_to_byte(clamp_unit(floats[0]) * 255.0f);
            color.g = clamp_to_byte(clamp_unit(floats[1]) * 255.0f);
            color.b = clamp_to_byte(clamp_unit(floats[2]) * 255.0f);
            color.a = clamp_to_byte(clamp_unit(floats[3]) * 255.0f);
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R16:
            color.r = clamp_to_byte(clamp_unit(half_to_float(shorts[0])) * 255.0f);
            color.g = color.r;
            color.b = color.r;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R16G16B16:
            color.r = clamp_to_byte(clamp_unit(half_to_float(shorts[0])) * 255.0f);
            color.g = clamp_to_byte(clamp_unit(half_to_float(shorts[1])) * 255.0f);
            color.b = clamp_to_byte(clamp_unit(half_to_float(shorts[2])) * 255.0f);
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R16G16B16A16:
            color.r = clamp_to_byte(clamp_unit(half_to_float(shorts[0])) * 255.0f);
            color.g = clamp_to_byte(clamp_unit(half_to_float(shorts[1])) * 255.0f);
            color.b = clamp_to_byte(clamp_unit(half_to_float(shorts[2])) * 255.0f);
            color.a = clamp_to_byte(clamp_unit(half_to_float(shorts[3])) * 255.0f);
            break;
        default:
            break; /* compressed: nothing to read */
    }

    return color;
}

void graphic_pixel_color_set(void           *destination,
                             graphic_color_t color,
                             int             format)
{
    uint8_t  *bytes  = (uint8_t*)destination;
    uint16_t *shorts = (uint16_t*)destination;
    float    *floats = (float*)destination;

    if (destination == NULL)
        return;

    switch (format) {
        case GRAPHIC_PIXEL_UNCOMPRESSED_GRAYSCALE:
            /* the usual luminance weights, not a plain average */
            bytes[0] = clamp_to_byte((float)color.r * 0.299f +
                                     (float)color.g * 0.587f +
                                     (float)color.b * 0.114f);
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_GRAY_ALPHA:
            bytes[0] = clamp_to_byte((float)color.r * 0.299f +
                                     (float)color.g * 0.587f +
                                     (float)color.b * 0.114f);
            bytes[1] = color.a;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R5G6B5:
            shorts[0] = (uint16_t)(((color.r >> 3) << 11) |
                                   ((color.g >> 2) <<  5) | (color.b >> 3));
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R5G5B5A1:
            shorts[0] = (uint16_t)(((color.r >> 3) << 11) |
                                   ((color.g >> 3) <<  6) |
                                   ((color.b >> 3) <<  1) |
                                   ((color.a > 127) ? 1 : 0));
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R4G4B4A4:
            shorts[0] = (uint16_t)(((color.r >> 4) << 12) |
                                   ((color.g >> 4) <<  8) |
                                   ((color.b >> 4) <<  4) | (color.a >> 4));
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8:
            bytes[0] = color.r;
            bytes[1] = color.g;
            bytes[2] = color.b;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R8G8B8A8:
            bytes[0] = color.r;
            bytes[1] = color.g;
            bytes[2] = color.b;
            bytes[3] = color.a;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R32:
            floats[0] = (float)color.r / 255.0f;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R32G32B32:
            floats[0] = (float)color.r / 255.0f;
            floats[1] = (float)color.g / 255.0f;
            floats[2] = (float)color.b / 255.0f;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R32G32B32A32:
            floats[0] = (float)color.r / 255.0f;
            floats[1] = (float)color.g / 255.0f;
            floats[2] = (float)color.b / 255.0f;
            floats[3] = (float)color.a / 255.0f;
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R16:
            shorts[0] = float_to_half((float)color.r / 255.0f);
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R16G16B16:
            shorts[0] = float_to_half((float)color.r / 255.0f);
            shorts[1] = float_to_half((float)color.g / 255.0f);
            shorts[2] = float_to_half((float)color.b / 255.0f);
            break;
        case GRAPHIC_PIXEL_UNCOMPRESSED_R16G16B16A16:
            shorts[0] = float_to_half((float)color.r / 255.0f);
            shorts[1] = float_to_half((float)color.g / 255.0f);
            shorts[2] = float_to_half((float)color.b / 255.0f);
            shorts[3] = float_to_half((float)color.a / 255.0f);
            break;
        default:
            break;
    }
}