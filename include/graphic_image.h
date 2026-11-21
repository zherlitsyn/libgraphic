#ifndef GRAPHIC_IMAGE_H
#define GRAPHIC_IMAGE_H

#include <stdbool.h>

#include "graphic_color.h"
#include "graphic_linalg.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CPU side images: pixels in main memory, in any of the uncompressed
 * formats. Nothing in this header touches the GPU; uploading one of
 * these becomes a texture, which is a separate module.
 *
 * File format support is deliberately thin. BMP is read and written
 * here because it is a hundred lines. PNG, JPEG and the rest are
 * decoded by stb_image.h when GRAPHIC_USE_STB_IMAGE is on (the
 * default), and PNG is written when stb_image_write.h is present.
 * Both headers ship in third_party/.
 */

typedef struct graphic_image {
    void *data;
    int   width;
    int   height;
    int   mipmaps; /* 1 means no mip chain */
    int   format;  /* enum graphic_pixel_format */
} graphic_image_t;

enum graphic_gradient_direction {
    GRAPHIC_GRADIENT_HORIZONTAL = 0,
    GRAPHIC_GRADIENT_VERTICAL,
};

/* ------------------------------------------------------------------ */
/* loading and saving                                                 */
/* ------------------------------------------------------------------ */

graphic_image_t graphic_image_load             (const char *file_name);

graphic_image_t graphic_image_load_raw         (const char *file_name,
                                                int         width,
                                                int         height,
                                                int         format,
                                                int         header_size);

graphic_image_t graphic_image_load_from_memory (const char          *file_type,
                                                const unsigned char *file_data,
                                                int                  data_size);

bool            graphic_image_is_valid         (graphic_image_t image);

void            graphic_image_free             (graphic_image_t image);

bool            graphic_image_export           (graphic_image_t image,
                                                const char     *file_name);

unsigned char  *graphic_image_export_to_memory (graphic_image_t image,
                                                const char     *file_type,
                                                int            *file_size);

bool            graphic_image_export_as_code   (graphic_image_t image,
                                                const char     *file_name);

/* ------------------------------------------------------------------ */
/* generation                                                         */
/* ------------------------------------------------------------------ */

graphic_image_t graphic_image_generate_color           (int             width,
                                                        int             height,
                                                        graphic_color_t color);

graphic_image_t graphic_image_generate_gradient_linear (int             width,
                                                        int             height,
                                                        int             direction,
                                                        graphic_color_t start,
                                                        graphic_color_t end);

graphic_image_t graphic_image_generate_gradient_radial (int             width,
                                                        int             height,
                                                        float           density,
                                                        graphic_color_t inner,
                                                        graphic_color_t outer);

graphic_image_t graphic_image_generate_gradient_square (int             width,
                                                        int             height,
                                                        float           density,
                                                        graphic_color_t inner,
                                                        graphic_color_t outer);

graphic_image_t graphic_image_generate_checked (int             width,
                                                int             height,
                                                int             checks_x,
                                                int             checks_y,
                                                graphic_color_t first,
                                                graphic_color_t second);

graphic_image_t graphic_image_generate_white_noise     (int   width,
                                                        int   height,
                                                        float factor);

graphic_image_t graphic_image_generate_perlin_noise    (int   width,
                                                        int   height,
                                                        int   offset_x,
                                                        int   offset_y,
                                                        float scale);

graphic_image_t graphic_image_generate_cellular        (int width,
                                                        int height,
                                                        int tile_size);

/* ------------------------------------------------------------------ */
/* queries                                                            */
/* ------------------------------------------------------------------ */

graphic_color_t graphic_image_color_get(graphic_image_t image, int x, int y);

/* ------------------------------------------------------------------ */
/* whole image operations                                             */
/*                                                                    */
/* The ones taking a pointer rewrite the image in place, freeing and  */
/* reallocating its pixels as needed.                                 */
/* ------------------------------------------------------------------ */

graphic_image_t graphic_image_copy         (graphic_image_t image);

graphic_image_t graphic_image_from_channel (graphic_image_t image,
                                            int             channel);

void            graphic_image_format       (graphic_image_t *image,
                                            int              new_format);

#ifdef __cplusplus
}
#endif

#endif /* GRAPHIC_IMAGE_H */