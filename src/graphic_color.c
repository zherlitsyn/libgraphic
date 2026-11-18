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