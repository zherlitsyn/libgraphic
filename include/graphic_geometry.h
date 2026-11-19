#ifndef GRAPHIC_GEOMETRY_H
#define GRAPHIC_GEOMETRY_H

#include <stdbool.h>

#include "graphic_linalg.h"

/*
 * Spline evaluation and 2D intersection tests.
 *
 * Nothing here draws, which is why the names say what they answer
 * rather than what they paint: graphic_point_in_rect() is a question,
 * graphic_draw_rectangle_rec() is a command.
 */

/* ------------------------------------------------------------------ */
/* splines                                                             */
/*                                                                     */
/* t runs from 0 to 1 along one segment. Basis and Catmull-Rom take    */
/* four control points and interpolate between the middle two; the     */
/* Bezier variants pass through their end points.                      */
/* ------------------------------------------------------------------ */

vec2_t graphic_spline_point_linear_get(vec2_t start,
                                       vec2_t end,
                                       float  t);

vec2_t graphic_spline_point_basis_get(vec2_t p1,
                                      vec2_t p2,
                                      vec2_t p3,
                                      vec2_t p4,
                                      float  t);

vec2_t graphic_spline_point_catmull_rom_get(vec2_t p1,
                                            vec2_t p2,
                                            vec2_t p3,
                                            vec2_t p4,
                                            float  t);

vec2_t graphic_spline_point_bezier_quad_get(vec2_t p1,
                                            vec2_t c2,
                                            vec2_t p3,
                                            float  t);

vec2_t graphic_spline_point_bezier_cubic_get(vec2_t p1,
                                             vec2_t c2,
                                             vec2_t c3,
                                             vec2_t p4,
                                             float  t);

/* ------------------------------------------------------------------ */
/* intersection                                                        */
/* ------------------------------------------------------------------ */

bool graphic_rect_collide_rect(rect_t a,
                               rect_t b);

bool graphic_circle_collide_circle(vec2_t center1,
                                   float  radius1,
                                   vec2_t center2,
                                   float  radius2);

bool graphic_circle_collide_rect(vec2_t center,
                                 float  radius,
                                 rect_t rectangle);

bool graphic_circle_collide_line(vec2_t center,
                                 float  radius, 
                                 vec2_t p1,
                                 vec2_t p2);

bool graphic_point_in_rect(vec2_t point,
                           rect_t rectangle);

bool graphic_point_in_circle(vec2_t point,
                             vec2_t center,
                             float  radius);

bool graphic_point_in_triangle(vec2_t point,
                               vec2_t p1,
                               vec2_t p2,
                               vec2_t p3);

bool graphic_point_in_poly(vec2_t        point,
                           const vec2_t *points,
                           int           point_count);

bool graphic_point_on_line(vec2_t point,
                           vec2_t p1,
                           vec2_t p2,
                           int    threshold);

/* collision_point may be NULL when only the yes or no answer is wanted */
bool graphic_line_collide_line(vec2_t  start1,
                               vec2_t  end1,
                               vec2_t  start2,
                               vec2_t  end2,
                               vec2_t *collision_point);

/* the overlapping part of two rectangles, zero sized when they miss */
rect_t graphic_rect_intersect_get(rect_t a,
                                  rect_t b);

#endif /* GRAPHIC_GEOMETRY_H */