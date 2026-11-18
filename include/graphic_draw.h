#ifndef GRAPHIC_DRAW_H
#define GRAPHIC_DRAW_H

#include <stdbool.h>
#include <stdint.h>

#include "graphic_color.h"
#include "graphic_linalg.h"
#include "graphic_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 2D shape drawing, built entirely on the immediate mode calls in
 * graphic.h. Nothing here talks to the backend: every function ends up
 * as graphic_begin, some vertices and graphic_end, so shapes batch
 * together with everything else drawn in the same frame.
 *
 * Coordinates are in the space of whatever mode is active. Inside
 * graphic_mode2d_begin() that is world space; outside it, pixels with
 * the origin at the top left.
 */

/* ------------------------------------------------------------------ */
/* points and lines                                                   */
/* ------------------------------------------------------------------ */

void graphic_draw_pixel(int x, int y, graphic_color_t color);

void graphic_draw_pixel_v(vec2_t position, graphic_color_t color);

void graphic_draw_line(int             start_x,
                       int             start_y,
                       int             end_x,
                       int             end_y,
                       graphic_color_t color);

void graphic_draw_line_v(vec2_t start, vec2_t end, graphic_color_t color);

void graphic_draw_line_ex(vec2_t          start,
                          vec2_t          end,
                          float           thick,
                          graphic_color_t color);

void graphic_draw_line_strip(const vec2_t   *points,
                             int             point_count,
                             graphic_color_t color);

void graphic_draw_line_dashed(vec2_t          start,
                              vec2_t          end,
                              int             dash_size,
                              int             space_size,
                              graphic_color_t color);

/* ------------------------------------------------------------------ */
/* circles, ellipses and rings                                        */
/* ------------------------------------------------------------------ */

void graphic_draw_circle              (int             center_x,
                                       int             center_y,
                                       float           radius,
                                       graphic_color_t color);

void graphic_draw_circle_v            (vec2_t          center,
                                       float           radius,
                                       graphic_color_t color);

void graphic_draw_circle_gradient     (vec2_t          center,
                                       float           radius,
                                       graphic_color_t inner,
                                       graphic_color_t outer);

void graphic_draw_circle_sector       (vec2_t          center,
                                       float           radius,
                                       float           start_angle,
                                       float           end_angle,
                                       int             segments,
                                       graphic_color_t color);

void graphic_draw_circle_sector_lines (vec2_t          center,
                                       float           radius,
                                       float           start_angle,
                                       float           end_angle,
                                       int             segments,
                                       graphic_color_t color);

void graphic_draw_circle_lines        (int             center_x,
                                       int             center_y,
                                       float           radius,
                                       graphic_color_t color);

void graphic_draw_circle_lines_v      (vec2_t          center,
                                       float           radius,
                                       graphic_color_t color);

void graphic_draw_ellipse             (int             center_x,
                                       int             center_y,
                                       float           radius_h,
                                       float           radius_v,
                                       graphic_color_t color);

void graphic_draw_ellipse_v           (vec2_t          center,
                                       float           radius_h,
                                       float           radius_v,
                                       graphic_color_t color);

void graphic_draw_ellipse_lines       (int             center_x,
                                       int             center_y,
                                       float           radius_h,
                                       float           radius_v,
                                       graphic_color_t color);

void graphic_draw_ellipse_lines_v     (vec2_t          center,
                                       float           radius_h,
                                       float           radius_v,
                                       graphic_color_t color);

void graphic_draw_ring                (vec2_t          center,
                                       float           inner_radius,
                                       float           outer_radius,
                                       float           start_angle,
                                       float           end_angle,
                                       int             segments,
                                       graphic_color_t color);

void graphic_draw_ring_lines          (vec2_t          center,
                                       float           inner_radius,
                                       float           outer_radius,
                                       float           start_angle,
                                       float           end_angle,
                                       int             segments,
                                       graphic_color_t color);

/* ------------------------------------------------------------------ */
/* rectangles                                                         */
/* ------------------------------------------------------------------ */

void graphic_draw_rectangle             (int             x,
                                         int             y,
                                         int             width,
                                         int             height,
                                         graphic_color_t color);

void graphic_draw_rectangle_v           (vec2_t          position,
                                         vec2_t          size,
                                         graphic_color_t color);

void graphic_draw_rectangle_rec         (rect_t rectangle, graphic_color_t color);

void graphic_draw_rectangle_pro         (rect_t          rectangle,
                                         vec2_t          origin,
                                         float           rotation,
                                         graphic_color_t color);

void graphic_draw_rectangle_gradient_v  (int             x,
                                         int             y,
                                         int             width,
                                         int             height,
                                         graphic_color_t top,
                                         graphic_color_t bottom);

void graphic_draw_rectangle_gradient_h  (int             x,
                                         int             y,
                                         int             width,
                                         int             height,
                                         graphic_color_t left,
                                         graphic_color_t right);

void graphic_draw_rectangle_gradient_ex (rect_t          rectangle,
                                         graphic_color_t top_left,
                                         graphic_color_t bottom_left,
                                         graphic_color_t bottom_right,
                                         graphic_color_t top_right);

void graphic_draw_rectangle_lines       (int             x,
                                         int             y,
                                         int             width,
                                         int             height,
                                         graphic_color_t color);

void graphic_draw_rectangle_lines_ex    (rect_t          rectangle,
                                         float           line_thick,
                                         graphic_color_t color);

/*
 * Indices into the roundness array of the rounded rectangle calls. The
 * order is the one graphic_draw_rectangle_gradient_ex() takes its
 * corner colours in.
 */
enum graphic_corner {
    GRAPHIC_CORNER_TOP_LEFT = 0,
    GRAPHIC_CORNER_BOTTOM_LEFT,
    GRAPHIC_CORNER_BOTTOM_RIGHT,
    GRAPHIC_CORNER_TOP_RIGHT,
    GRAPHIC_CORNER_COUNT
};

/*
 * roundness holds one value per corner, indexed by enum graphic_corner.
 * Each runs from 0, a square corner, to 1, a radius of half the shorter
 * side. So two neighbouring corners can never overlap, and all four
 * at 1 gives fully rounded ends. NULL is a plain rectangle.
 */
void graphic_draw_rectangle_rounded          (rect_t          rectangle,
                                              const float     roundness[4],
                                              int             segments,
                                              graphic_color_t color);
/*
 * A rounded rectangle whose four corners carry four colours, blended
 * across it the way graphic_draw_rectangle_gradient_ex() blends a
 * square one. No shader: the batch already interpolates vertex
 * colours, so this only has to work out the colour at each vertex it
 * emits.
 *
 * Drawn as one fan from the middle, like the solid version. A fan has
 * no diagonal seam, which a quad split into two triangles does show
 * when its four corners differ enough, and it puts the whole shape in
 * one run of triangles.
 */
void graphic_draw_rectangle_rounded_gradient (rect_t          rectangle,
                                              float           roundness,
                                              int             segments,
                                              graphic_color_t top_left,
                                              graphic_color_t bottom_left,
                                              graphic_color_t bottom_right,
                                              graphic_color_t top_right);

/*
 * The outline is centred on the edge of the rectangle, half the
 * thickness inside and half outside. The exception is all four corners
 * square (or roundness NULL), which draws graphic_draw_rectangle_lines_ex(),
 * whose border sits entirely inside.
 */
void graphic_draw_rectangle_rounded_lines    (rect_t          rectangle,
                                              const float     roundness[4],
                                              int             segments,
                                              graphic_color_t color);

void graphic_draw_rectangle_rounded_lines_ex (rect_t          rectangle,
                                              const float     roundness[4],
                                              int             segments,
                                              float           line_thick,
                                              graphic_color_t color);

/* ------------------------------------------------------------------ */
/* triangles and polygons                                             */
/* ------------------------------------------------------------------ */

void graphic_draw_triangle(vec2_t          v1,
                           vec2_t          v2,
                           vec2_t          v3,
                           graphic_color_t color);

void graphic_draw_triangle_lines(vec2_t          v1,
                                 vec2_t          v2,
                                 vec2_t          v3,
                                 graphic_color_t color);

void graphic_draw_triangle_fan(const vec2_t   *points,
                               int             point_count,
                               graphic_color_t color);

void graphic_draw_triangle_strip(const vec2_t   *points,
                                 int             point_count,
                                 graphic_color_t color);

void graphic_draw_poly(vec2_t          center,
                       int             sides,
                       float           radius,
                       float           rotation,
                       graphic_color_t color);

void graphic_draw_poly_lines(vec2_t          center,
                             int             sides,
                             float           radius,
                             float           rotation,
                             graphic_color_t color);

void graphic_draw_poly_lines_ex(vec2_t          center,
                                int             sides,
                                float           radius,
                                float           rotation,
                                float           line_thick,
                                graphic_color_t color);

#ifdef __cplusplus
}
#endif

#endif /* GRAPHIC_DRAW_H */