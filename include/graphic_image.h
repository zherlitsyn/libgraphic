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

graphic_color_t  graphic_image_color_get        (graphic_image_t image,
                                                 int             x,
                                                 int             y);

rect_t           graphic_image_alpha_border_get (graphic_image_t image,
                                                 float           threshold);

graphic_color_t *graphic_image_colors_load      (graphic_image_t image);

void             graphic_image_colors_free      (graphic_color_t *colors);

graphic_color_t *graphic_image_palette_load (graphic_image_t image,
                                             int             max_palette_size,
                                             int            *color_count);

void             graphic_image_palette_free     (graphic_color_t *colors);

/* ------------------------------------------------------------------ */
/* whole image operations                                             */
/*                                                                    */
/* The ones taking a pointer rewrite the image in place, freeing and  */
/* reallocating its pixels as needed.                                 */
/* ------------------------------------------------------------------ */

graphic_image_t graphic_image_copy              (graphic_image_t image);

graphic_image_t graphic_image_from_image        (graphic_image_t image,
                                                 rect_t          rectangle);

graphic_image_t graphic_image_from_channel      (graphic_image_t image,
                                                 int             channel);

void            graphic_image_format            (graphic_image_t *image,
                                                 int              new_format);

void            graphic_image_to_pot            (graphic_image_t *image,
                                                 graphic_color_t  fill);

void            graphic_image_crop              (graphic_image_t *image,
                                                 rect_t           crop);

void            graphic_image_alpha_crop        (graphic_image_t *image,
                                                 float            threshold);

void            graphic_image_alpha_clear       (graphic_image_t *image,
                                                 graphic_color_t  color,
                                                 float            threshold);

void            graphic_image_alpha_mask        (graphic_image_t *image,
                                                 graphic_image_t  alpha_mask);

void            graphic_image_alpha_premultiply (graphic_image_t *image);

void            graphic_image_resize            (graphic_image_t *image,
                                                 int              new_width,
                                                 int              new_height);

void            graphic_image_resize_nn         (graphic_image_t *image,
                                                 int              new_width,
                                                 int              new_height);

void            graphic_image_resize_canvas     (graphic_image_t *image,
                                                 int              new_width,
                                                 int              new_height,
                                                 int              offset_x,
                                                 int              offset_y,
                                                 graphic_color_t  fill);

void            graphic_image_flip_vertical     (graphic_image_t *image);

void            graphic_image_flip_horizontal   (graphic_image_t *image);

void            graphic_image_rotate            (graphic_image_t *image,
                                                 int              degrees);

void            graphic_image_rotate_cw         (graphic_image_t *image);

void            graphic_image_rotate_ccw        (graphic_image_t *image);

/* ------------------------------------------------------------------ */
/* drawing into an image                                              */
/*                                                                    */
/* The CPU twin of graphic_draw_*. Same shapes, no GPU involved, so   */
/* these work before a window exists and are the way to build an      */
/* image that will later become a texture.                            */
/* ------------------------------------------------------------------ */

void graphic_image_clear_background   (graphic_image_t *destination,
                                       graphic_color_t  color);

void graphic_image_draw_pixel         (graphic_image_t *destination,
                                       int              x,
                                       int              y,
                                       graphic_color_t  color);

void graphic_image_draw_pixel_v       (graphic_image_t *destination,
                                       vec2_t           position,
                                       graphic_color_t  color);

void graphic_image_draw_rectangle     (graphic_image_t *destination,
                                       int              x,
                                       int              y,
                                       int              width,
                                       int              height,
                                       graphic_color_t  color);

void graphic_image_draw_rectangle_v   (graphic_image_t *destination,
                                       vec2_t           position,
                                       vec2_t           size,
                                       graphic_color_t  color);

void graphic_image_draw_rectangle_rec (graphic_image_t *destination,
                                       rect_t           rectangle,
                                       graphic_color_t  color);

#ifdef __cplusplus
}
#endif

#endif /* GRAPHIC_IMAGE_H */