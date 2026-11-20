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