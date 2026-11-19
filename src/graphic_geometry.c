#include <math.h>
#include <stddef.h>

#include "graphic_geometry.h"

/* ------------------------------------------------------------------ */
/* splines                                                            */
/* ------------------------------------------------------------------ */

vec2_t graphic_spline_point_linear_get(vec2_t start,
                                       vec2_t end,
                                       float  t)
{
    vec2_t point;

    point.x = start.x * (1.0f - t) + end.x * t;
    point.y = start.y * (1.0f - t) + end.y * t;

    return point;
}

/*
 * Uniform cubic B-spline. The curve does not pass through its control
 * points, which is exactly what makes it smooth across joins.
 */
vec2_t graphic_spline_point_basis_get(vec2_t p1,
                                      vec2_t p2,
                                      vec2_t p3,
                                      vec2_t p4,
                                      float  t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    float a[4];
    a[0] = (-t3 + 3.0f * t2 - 3.0f * t + 1.0f) / 6.0f;
    a[1] = (3.0f * t3 - 6.0f * t2 + 4.0f) / 6.0f;
    a[2] = (-3.0f * t3 + 3.0f * t2 + 3.0f * t + 1.0f) / 6.0f;
    a[3] = t3 / 6.0f;

    float b[4];
    b[0] = p1.x;
    b[1] = p2.x;
    b[2] = p3.x;
    b[3] = p4.x;

    vec2_t point;
    point.x = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];

    b[0] = p1.y;
    b[1] = p2.y;
    b[2] = p3.y;
    b[3] = p4.y;

    point.y = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];

    return point;
}

/* passes through p2 and p3, using p1 and p4 only for the tangents */
vec2_t graphic_spline_point_catmull_rom_get(vec2_t p1,
                                            vec2_t p2,
                                            vec2_t p3,
                                            vec2_t p4,
                                            float  t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    float q0 = -t3 + 2.0f * t2 - t;
    float q1 = 3.0f * t3 - 5.0f * t2 + 2.0f;
    float q2 = -3.0f * t3 + 4.0f * t2 + t;
    float q3 = t3 - t2;

    vec2_t point;
    point.x = 0.5f * (p1.x * q0 + p2.x * q1 + p3.x * q2 + p4.x * q3);
    point.y = 0.5f * (p1.y * q0 + p2.y * q1 + p3.y * q2 + p4.y * q3);

    return point;
}

vec2_t graphic_spline_point_bezier_quad_get(vec2_t p1,
                                            vec2_t c2,
                                            vec2_t p3,
                                            float  t)
{
    float u = 1.0f - t;
    float a = u * u;
    float b = 2.0f * u * t;
    float c = t * t;

    vec2_t point;
    point.x = a * p1.x + b * c2.x + c * p3.x;
    point.y = a * p1.y + b * c2.y + c * p3.y;

    return point;
}

vec2_t graphic_spline_point_bezier_cubic_get(vec2_t p1,
                                             vec2_t c2,
                                             vec2_t c3,
                                             vec2_t p4,
                                             float  t)
{
    float u = 1.0f - t;
    float a = u * u * u;
    float b = 3.0f * u * u * t;
    float c = 3.0f * u * t * t;
    float d = t * t * t;

    vec2_t point;
    point.x = a * p1.x + b * c2.x + c * c3.x + d * p4.x;
    point.y = a * p1.y + b * c2.y + c * c3.y + d * p4.y;

    return point;
}

/* ------------------------------------------------------------------ */
/* intersection                                                       */
/* ------------------------------------------------------------------ */

bool graphic_rect_collide_rect(rect_t a,
                               rect_t b)
{
    return a.x < b.x + b.width  && a.x + a.width  > b.x &&
           a.y < b.y + b.height && a.y + a.height > b.y;
}

bool graphic_circle_collide_circle(vec2_t center1,
                                   float  radius1,
                                   vec2_t center2,
                                   float  radius2)
{
    float dx  = center2.x - center1.x;
    float dy  = center2.y - center1.y;
    float sum = radius1 + radius2;

    /* squared, to keep the square root out of the hot path */
    return dx * dx + dy * dy <= sum * sum;
}

bool graphic_circle_collide_rect(vec2_t center,
                                 float  radius,
                                 rect_t rectangle)
{
    float half_width  = rectangle.width  / 2.0f;
    float half_height = rectangle.height / 2.0f;
    float dx = fabsf(center.x - (rectangle.x + half_width));
    float dy = fabsf(center.y - (rectangle.y + half_height));

    if (dx > half_width + radius || dy > half_height + radius)
        return false;

    /* inside the cross made by the rectangle extended by the radius */
    if (dx <= half_width || dy <= half_height)
        return true;

    /* the only case left is the rounded corner */
    float corner_x = dx - half_width;
    float corner_y = dy - half_height;

    return corner_x * corner_x + corner_y * corner_y <= radius * radius;
}

bool graphic_circle_collide_line(vec2_t center,
                                 float  radius,
                                 vec2_t p1,
                                 vec2_t p2)
{
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float length_squared = dx * dx + dy * dy;

    if (length_squared < MATH_EPSILON)
        return graphic_point_in_circle(p1, center, radius);

    /* project the centre onto the segment and clamp to its ends */
    float t = ((center.x - p1.x) * dx + (center.y - p1.y) * dy) / length_squared;

    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    vec2_t closest;
    closest.x = p1.x + t * dx;
    closest.y = p1.y + t * dy;

    return graphic_point_in_circle(closest, center, radius);
}

bool graphic_point_in_rect(vec2_t point,
                           rect_t rectangle)
{
    return point.x >= rectangle.x &&
           point.x < rectangle.x + rectangle.width &&
           point.y >= rectangle.y &&
           point.y < rectangle.y + rectangle.height;
}

bool graphic_point_in_circle(vec2_t point,
                             vec2_t center,
                             float  radius)
{
    float dx = point.x - center.x;
    float dy = point.y - center.y;

    return dx * dx + dy * dy <= radius * radius;
}

/* barycentric sign test, works for either winding */
bool graphic_point_in_triangle(vec2_t point,
                               vec2_t p1,
                               vec2_t p2,
                               vec2_t p3)
{
    float denominator = (p2.y - p3.y) * (p1.x - p3.x) +
                        (p3.x - p2.x) * (p1.y - p3.y);

    if (fabsf(denominator) < MATH_EPSILON)
        return false;        /* degenerate triangle */

    float alpha = ((p2.y - p3.y) * (point.x - p3.x) +
                   (p3.x - p2.x) * (point.y - p3.y)) / denominator;

    float beta  = ((p3.y - p1.y) * (point.x - p3.x) +
                   (p1.x - p3.x) * (point.y - p3.y)) / denominator;

    float gamma = 1.0f - alpha - beta;

    return alpha >= 0.0f && beta >= 0.0f && gamma >= 0.0f;
}

/* even odd ray casting, so self intersecting outlines behave sanely */
bool graphic_point_in_poly(vec2_t        point,
                           const vec2_t *points,
                           int           point_count)
{
    bool inside = false;

    if (points == NULL || point_count < 3)
        return false;

    for (int i = 0, j = point_count - 1; i < point_count; j = i++) {
        if ((points[i].y > point.y) == (points[j].y > point.y))
            continue;

        if (point.x < (points[j].x - points[i].x) * (point.y - points[i].y) / 
                      (points[j].y - points[i].y) + points[i].x)
            inside = !inside;
    }

    return inside;
}

bool graphic_point_on_line(vec2_t point,
                           vec2_t p1,
                           vec2_t p2,
                           int    threshold)
{
    float dx             = p2.x - p1.x;
    float dy             = p2.y - p1.y;
    float length_squared = dx * dx + dy * dy;

    if (length_squared < MATH_EPSILON)
        return graphic_point_in_circle(point, p1, (float)threshold);

    float t = ((point.x - p1.x) * dx + (point.y - p1.y) * dy) / length_squared;

    if (t < 0.0f || t > 1.0f)
        return false;        /* beyond the ends of the segment */

    float offset_x   = point.x - (p1.x + t * dx);
    float offset_y   = point.y - (p1.y + t * dy);
    float threshold2 = (float)threshold * (float)threshold;

    return offset_x * offset_x + offset_y * offset_y <= threshold2;
}

bool graphic_line_collide_line(vec2_t  start1,
                               vec2_t  end1,
                               vec2_t  start2,
                               vec2_t  end2,
                               vec2_t *collision_point)
{
    float dx1         = end1.x - start1.x;
    float dy1         = end1.y - start1.y;
    float dx2         = end2.x - start2.x;
    float dy2         = end2.y - start2.y;
    float denominator = dx1 * dy2 - dy1 * dx2;

    if (fabsf(denominator) < MATH_EPSILON)
        return false;        /* parallel or coincident */

    float t = ((start2.x - start1.x) * dy2 - (start2.y - start1.y) * dx2) /
              denominator;

    float u = ((start2.x - start1.x) * dy1 - (start2.y - start1.y) * dx1) /
              denominator;

    if (t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f)
        return false;

    if (collision_point != NULL) {
        collision_point->x = start1.x + t * dx1;
        collision_point->y = start1.y + t * dy1;
    }

    return true;
}

rect_t graphic_rect_intersect_get(rect_t a,
                                  rect_t b)
{
    rect_t result = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (!graphic_rect_collide_rect(a, b))
        return result;

    float left   = (a.x > b.x) ? a.x : b.x;
    float top    = (a.y > b.y) ? a.y : b.y;
    float right  = (a.x + a.width < b.x + b.width)   ? a.x + a.width  : b.x + b.width;
    float bottom = (a.y + a.height < b.y + b.height) ? a.y + a.height : b.y + b.height;

    result.x = left;
    result.y = top;
    result.width = right - left;
    result.height = bottom - top;

    return result;
}