#include <math.h>
#include <stddef.h>

#include "graphic_internal.h"

/*
 * 2D shapes on top of the immediate mode calls. Not one line of this
 * file knows about Vulkan: everything becomes graphic_begin, vertices
 * and graphic_end, so shapes share the frame batch with 3D geometry
 * and cost no extra submit.
 */

/*
 * How far a chord may sit from the true arc, in pixels, before another
 * segment is added. Half a pixel is below what a display can show.
 */
#define SMOOTH_CIRCLE_ERROR_RATE    0.5f

/* ------------------------------------------------------------------ */
/* helpers                                                            */
/* ------------------------------------------------------------------ */

/*
 * Called by every shape before its vertices, so that one place decides
 * what a shape's colour becomes in the batch.
 */
static void color_apply(graphic_color_t color)
{
    graphic_color4ub(color.r, color.g, color.b, color.a);
}

/*
 * Segments needed so the chord never strays more than half a pixel
 * from the arc. A big circle gets more segments, a tiny one very few.
 */
static int circle_segments_get(float radius, float sweep_degrees)
{
    /*
     * Below half a pixel any four points meet the error bound. The
     * formula must not see such a radius either: from a quarter pixel
     * down its cosine reaches one, the angle zero, and the division by
     * that angle made an infinity that was then converted to int,
     * which C leaves undefined.
     */
    if (radius <= SMOOTH_CIRCLE_ERROR_RATE)
        return 4;

    float threshold = 2.0f * (1.0f - SMOOTH_CIRCLE_ERROR_RATE / radius) *
                     (1.0f - SMOOTH_CIRCLE_ERROR_RATE / radius) - 1.0f;

    int segments = (int)(ceilf(2.0f * MATH_PI /
                         acosf(threshold)) * sweep_degrees / 360.0f);

    if (segments < 4)
        segments = 4;

    if (segments > 512)
        segments = 512;

    return segments;
}

static vec2_t point_on_arc(vec2_t center,
                           float  radius_h,
                           float  radius_v,
                           float  angle_degrees)
{
    float angle = angle_degrees * DEG2RAD;

    vec2_t point;
    point.x = center.x + cosf(angle) * radius_h;
    point.y = center.y + sinf(angle) * radius_v;

    return point;
}

/*
 * An arc as pairs of line vertices, inside a graphic_begin() of lines
 * the caller has opened. Fewer than four segments means the count is
 * worked out from the radius.
 */
static void arc_lines_add(vec2_t center,
                          float  radius,
                          float  start_angle,
                          float  sweep,
                          int    segments)
{
    if (segments < 4)
        segments = circle_segments_get(radius, sweep);

    float step  = sweep / (float)segments;
    float angle = start_angle;

    for (int i = 0; i < segments; i++) {
        vec2_t a = point_on_arc(center, radius, radius, angle);
        vec2_t b = point_on_arc(center, radius, radius, angle + step);

        graphic_vertex2f(a.x, a.y);
        graphic_vertex2f(b.x, b.y);

        angle += step;
    }
}

/* ------------------------------------------------------------------ */
/* points and lines                                                   */
/* ------------------------------------------------------------------ */

void graphic_draw_pixel_v(vec2_t position, graphic_color_t color)
{
    /*
     * A one pixel line rather than a point primitive: the pipeline
     * has no point topology, and one more pipeline for single
     * pixels would not pay for itself.
     */
    graphic_begin(GRAPHIC_DRAW_LINES);
    color_apply(color);
    graphic_vertex2f(position.x, position.y);
    graphic_vertex2f(position.x + 1.0f, position.y + 1.0f);
    graphic_end();
}

void graphic_draw_pixel(int x, int y, graphic_color_t color)
{
    vec2_t position = { (float)x, (float)y };

    graphic_draw_pixel_v(position, color);
}

void graphic_draw_line_v(vec2_t start, vec2_t end, graphic_color_t color)
{
    graphic_begin(GRAPHIC_DRAW_LINES);
    color_apply(color);
    graphic_vertex2f(start.x, start.y);
    graphic_vertex2f(end.x, end.y);
    graphic_end();
}

void graphic_draw_line(int             start_x,
                       int             start_y,
                       int             end_x,
                       int             end_y,
                       graphic_color_t color)
{
    vec2_t start = { (float)start_x, (float)start_y };
    vec2_t end   = { (float)end_x, (float)end_y };

    graphic_draw_line_v(start, end, color);
}

/*
 * A thick line is a quad, not a line primitive: line width above 1.0
 * needs the wideLines feature, which V3DV and much of mobile lack.
 */
void graphic_draw_line_ex(vec2_t          start,
                          vec2_t          end,
                          float           thick,
                          graphic_color_t color)
{
    float  dx     = end.x - start.x;
    float  dy     = end.y - start.y;
    float  length = sqrtf(dx * dx + dy * dy);
    
    if (length < MATH_EPSILON || thick <= 0.0f)
        return;

    vec2_t offset;
    offset.x = thick * dy / length / 2.0f;
    offset.y = -thick * dx / length / 2.0f;

    graphic_begin(GRAPHIC_DRAW_QUADS);
    color_apply(color);
    graphic_vertex2f(start.x - offset.x, start.y - offset.y);
    graphic_vertex2f(start.x + offset.x, start.y + offset.y);
    graphic_vertex2f(end.x + offset.x, end.y + offset.y);
    graphic_vertex2f(end.x - offset.x, end.y - offset.y);
    graphic_end();
}

void graphic_draw_line_strip(const vec2_t   *points,
                             int             point_count,
                             graphic_color_t color)
{
    if (points == NULL || point_count < 2)
        return;

    graphic_begin(GRAPHIC_DRAW_LINES);
    color_apply(color);

    /* the batch has no strip topology, so the joins are duplicated */
    for (int i = 0; i < point_count - 1; i++) {
        graphic_vertex2f(points[i].x, points[i].y);
        graphic_vertex2f(points[i + 1].x, points[i + 1].y);
    }

    graphic_end();
}

void graphic_draw_line_dashed(vec2_t          start,
                              vec2_t          end,
                              int             dash_size,
                              int             space_size,
                              graphic_color_t color)
{
    float  dx        = end.x - start.x;
    float  dy        = end.y - start.y;
    float  length    = sqrtf(dx * dx + dy * dy);
    float  period    = (float)(dash_size + space_size);
    
    if (length < MATH_EPSILON || dash_size <= 0 || period <= 0.0f)
        return;

    vec2_t direction;
    direction.x = dx / length;
    direction.y = dy / length;

    graphic_begin(GRAPHIC_DRAW_LINES);
    color_apply(color);

    float  travelled = 0.0f;
    while (travelled < length) {
        float dash_end = travelled + (float)dash_size;

        if (dash_end > length)
            dash_end = length;

        graphic_vertex2f(start.x + direction.x * travelled,
                         start.y + direction.y * travelled);
        graphic_vertex2f(start.x + direction.x * dash_end,
                         start.y + direction.y * dash_end);

        travelled += period;
    }

    graphic_end();
}

/* ------------------------------------------------------------------ */
/* circles, ellipses and rings                                        */
/* ------------------------------------------------------------------ */

void graphic_draw_circle_sector(vec2_t          center,
                                float           radius,
                                float           start_angle,
                                float           end_angle,
                                int             segments,
                                graphic_color_t color)
{
    if (radius <= 0.0f)
        return;

    float sweep = end_angle - start_angle;

    if (sweep < 0.0f) {
        float swap = start_angle;

        start_angle = end_angle;
        end_angle   = swap;
        sweep       = -sweep;
    }

    if (segments < 4)
        segments = circle_segments_get(radius, sweep);

    float step  = sweep / (float)segments;
    float angle = start_angle;

    graphic_begin(GRAPHIC_DRAW_TRIANGLES);
    color_apply(color);

    /* a fan around the centre, spelt out: the batch has no fans */
    for (int i = 0; i < segments; i++) {
        vec2_t a = point_on_arc(center, radius, radius, angle);
        vec2_t b = point_on_arc(center, radius, radius, angle + step);

        graphic_vertex2f(center.x, center.y);
        graphic_vertex2f(a.x, a.y);
        graphic_vertex2f(b.x, b.y);

        angle += step;
    }

    graphic_end();
}

void graphic_draw_circle_v(vec2_t          center,
                           float           radius,
                           graphic_color_t color)
{
    graphic_draw_circle_sector(center, radius, 0.0f, 360.0f, 0, color);
}

void graphic_draw_circle(int             center_x,
                         int             center_y,
                         float           radius,
                         graphic_color_t color)
{
    vec2_t center = { (float)center_x, (float)center_y };

    graphic_draw_circle_v(center, radius, color);
}

void graphic_draw_circle_gradient(vec2_t          center,
                                  float           radius,
                                  graphic_color_t inner,
                                  graphic_color_t outer)
{
    if (radius <= 0.0f)
        return;

    int   segments = circle_segments_get(radius, 360.0f);
    float step     = 360.0f / (float)segments;
    float angle    = 0.0f;

    graphic_begin(GRAPHIC_DRAW_TRIANGLES);

    for (int i = 0; i < segments; i++) {
        vec2_t a = point_on_arc(center, radius, radius, angle);
        vec2_t b = point_on_arc(center, radius, radius, angle + step);

        /* the colour is per vertex, so the shader interpolates it */
        color_apply(inner);
        graphic_vertex2f(center.x, center.y);

        color_apply(outer);
        graphic_vertex2f(a.x, a.y);
        graphic_vertex2f(b.x, b.y);

        angle += step;
    }

    graphic_end();
}

void graphic_draw_circle_sector_lines(vec2_t          center,
                                      float           radius,
                                      float           start_angle,
                                      float           end_angle,
                                      int             segments,
                                      graphic_color_t color)
{
    if (radius <= 0.0f)
        return;

    float sweep = end_angle - start_angle;

    if (sweep < 0.0f) {
        float swap = start_angle;

        start_angle = end_angle;
        end_angle   = swap;
        sweep       = -sweep;
    }

    graphic_begin(GRAPHIC_DRAW_LINES);
    color_apply(color);

    /* an open sector also needs the two radii drawn */
    if (sweep < 360.0f - MATH_EPSILON) {
        vec2_t first = point_on_arc(center, radius, radius, start_angle);
        vec2_t last  = point_on_arc(center, radius, radius, end_angle);

        graphic_vertex2f(center.x, center.y);
        graphic_vertex2f(first.x, first.y);
        graphic_vertex2f(center.x, center.y);
        graphic_vertex2f(last.x, last.y);
    }

    arc_lines_add(center, radius, start_angle, sweep, segments);

    graphic_end();
}

void graphic_draw_circle_lines_v(vec2_t          center,
                                 float           radius,
                                 graphic_color_t color)
{
    graphic_draw_circle_sector_lines(center, radius, 0.0f, 360.0f, 0, color);
}

void graphic_draw_circle_lines(int             center_x,
                               int             center_y,
                               float           radius,
                               graphic_color_t color)
{
    vec2_t center = { (float)center_x, (float)center_y };

    graphic_draw_circle_lines_v(center, radius, color);
}

void graphic_draw_ellipse_v(vec2_t          center,
                            float           radius_h,
                            float           radius_v,
                            graphic_color_t color)
{
    if (radius_h <= 0.0f || radius_v <= 0.0f)
        return;

    float largest  = (radius_h > radius_v) ? radius_h : radius_v;
    int   segments = circle_segments_get(largest, 360.0f);
    float step     = 360.0f / (float)segments;
    float angle    = 0.0f;

    graphic_begin(GRAPHIC_DRAW_TRIANGLES);
    color_apply(color);

    for (int i = 0; i < segments; i++) {
        vec2_t a = point_on_arc(center, radius_h, radius_v, angle);
        vec2_t b = point_on_arc(center, radius_h, radius_v, angle + step);

        graphic_vertex2f(center.x, center.y);
        graphic_vertex2f(a.x, a.y);
        graphic_vertex2f(b.x, b.y);

        angle += step;
    }

    graphic_end();
}

void graphic_draw_ellipse(int             center_x,
                          int             center_y,
                          float           radius_h,
                          float           radius_v,
                          graphic_color_t color)
{
    vec2_t center = { (float)center_x, (float)center_y };

    graphic_draw_ellipse_v(center, radius_h, radius_v, color);
}

void graphic_draw_ellipse_lines_v(vec2_t          center,
                                  float           radius_h,
                                  float           radius_v,
                                  graphic_color_t color)
{
    if (radius_h <= 0.0f || radius_v <= 0.0f)
        return;

    float largest  = (radius_h > radius_v) ? radius_h : radius_v;
    int   segments = circle_segments_get(largest, 360.0f);
    float step     = 360.0f / (float)segments;
    float angle    = 0.0f;

    graphic_begin(GRAPHIC_DRAW_LINES);
    color_apply(color);

    for (int i = 0; i < segments; i++) {
        vec2_t a = point_on_arc(center, radius_h, radius_v, angle);
        vec2_t b = point_on_arc(center, radius_h, radius_v, angle + step);

        graphic_vertex2f(a.x, a.y);
        graphic_vertex2f(b.x, b.y);

        angle += step;
    }

    graphic_end();
}

void graphic_draw_ellipse_lines(int             center_x,
                                int             center_y,
                                float           radius_h,
                                float           radius_v,
                                graphic_color_t color)
{
    vec2_t center = { (float)center_x, (float)center_y };

    graphic_draw_ellipse_lines_v(center, radius_h, radius_v, color);
}

void graphic_draw_ring(vec2_t          center,
                       float           inner_radius,
                       float           outer_radius,
                       float           start_angle,
                       float           end_angle,
                       int             segments,
                       graphic_color_t color)
{
    if (outer_radius <= 0.0f)
        return;

    if (inner_radius < 0.0f)
        inner_radius = 0.0f;

    if (inner_radius >= outer_radius) {
        graphic_draw_circle_sector_lines(center,
                                         outer_radius,
                                         start_angle,
                                         end_angle,
                                         segments,
                                         color);
        return;
    }

    float sweep = end_angle - start_angle;

    if (sweep < 0.0f) {
        float swap = start_angle;

        start_angle = end_angle;
        end_angle   = swap;
        sweep       = -sweep;
    }

    if (segments < 4)
        segments = circle_segments_get(outer_radius, sweep);

    float step  = sweep / (float)segments;
    float angle = start_angle;

    graphic_begin(GRAPHIC_DRAW_QUADS);
    color_apply(color);

    for (int i = 0; i < segments; i++) {
        vec2_t inner_a = point_on_arc(center,
                                      inner_radius,
                                      inner_radius,
                                      angle);
        vec2_t outer_a = point_on_arc(center,
                                      outer_radius,
                                      outer_radius,
                                      angle);
        vec2_t outer_b = point_on_arc(center,
                                      outer_radius,
                                      outer_radius,
                                      angle + step);
        vec2_t inner_b = point_on_arc(center,
                                      inner_radius,
                                      inner_radius,
                                      angle + step);

        graphic_vertex2f(inner_a.x, inner_a.y);
        graphic_vertex2f(outer_a.x, outer_a.y);
        graphic_vertex2f(outer_b.x, outer_b.y);
        graphic_vertex2f(inner_b.x, inner_b.y);

        angle += step;
    }

    graphic_end();
}

/*
 * The two arcs, joined at the ends when the ring is open. The arcs are
 * drawn bare: built from two sector outlines, as this function once
 * was, each brought its radii along and an open ring showed spokes
 * across its hole to the centre.
 */
void graphic_draw_ring_lines(vec2_t          center,
                             float           inner_radius,
                             float           outer_radius,
                             float           start_angle,
                             float           end_angle,
                             int             segments,
                             graphic_color_t color)
{
    if (outer_radius <= 0.0f)
        return;

    if (inner_radius < 0.0f)
        inner_radius = 0.0f;

    float sweep = end_angle - start_angle;

    if (sweep < 0.0f) {
        float swap = start_angle;

        start_angle = end_angle;
        end_angle   = swap;
        sweep       = -sweep;
    }

    graphic_begin(GRAPHIC_DRAW_LINES);
    color_apply(color);

    arc_lines_add(center, outer_radius, start_angle, sweep, segments);
    if (inner_radius > 0.0f)
        arc_lines_add(center, inner_radius, start_angle, sweep, segments);

    /* close the two open ends of the arc */
    if (sweep < 360.0f - MATH_EPSILON) {
        vec2_t inner_start = point_on_arc(center,
                                          inner_radius,
                                          inner_radius,
                                          start_angle);
        vec2_t outer_start = point_on_arc(center,
                                          outer_radius,
                                          outer_radius,
                                          start_angle);
        vec2_t inner_end = point_on_arc(center,
                                        inner_radius,
                                        inner_radius,
                                        end_angle);
        vec2_t outer_end = point_on_arc(center,
                                        outer_radius,
                                        outer_radius,
                                        end_angle);

        graphic_vertex2f(inner_start.x, inner_start.y);
        graphic_vertex2f(outer_start.x, outer_start.y);
        graphic_vertex2f(inner_end.x, inner_end.y);
        graphic_vertex2f(outer_end.x, outer_end.y);
    }

    graphic_end();
}

/* ------------------------------------------------------------------ */
/* rectangles                                                         */
/* ------------------------------------------------------------------ */

void graphic_draw_rectangle_gradient_ex(rect_t          rectangle,
                                        graphic_color_t top_left,
                                        graphic_color_t bottom_left,
                                        graphic_color_t bottom_right,
                                        graphic_color_t top_right)
{
    graphic_begin(GRAPHIC_DRAW_QUADS);

    color_apply(top_left);
    graphic_vertex2f(rectangle.x, rectangle.y);

    color_apply(bottom_left);
    graphic_vertex2f(rectangle.x, rectangle.y + rectangle.height);

    color_apply(bottom_right);
    graphic_vertex2f(rectangle.x + rectangle.width,
                     rectangle.y + rectangle.height);

    color_apply(top_right);
    graphic_vertex2f(rectangle.x + rectangle.width, rectangle.y);

    graphic_end();
}

void graphic_draw_rectangle_rec(rect_t rectangle, graphic_color_t color)
{
    graphic_draw_rectangle_gradient_ex(rectangle, color, color, color, color);
}

void graphic_draw_rectangle_v(vec2_t          position,
                              vec2_t          size,
                              graphic_color_t color)
{
    rect_t rectangle = { position.x, position.y, size.x, size.y };

    graphic_draw_rectangle_rec(rectangle, color);
}

void graphic_draw_rectangle(int             x,
                            int             y,
                            int             width,
                            int             height,
                            graphic_color_t color)
{
    rect_t rectangle = { (float)x, (float)y, (float)width, (float)height };

    graphic_draw_rectangle_rec(rectangle, color);
}

/*
 * origin is the pivot, given relative to the rectangle's own corner,
 * and rotation is applied about it. The corners are rotated here
 * rather than through the matrix stack so the call stays a single
 * quad and keeps batching with its neighbours.
 */
void graphic_draw_rectangle_pro(rect_t          rectangle,
                                vec2_t          origin,
                                float           rotation,
                                graphic_color_t color)
{
    float  sin_rotation = 0.0f;
    float  cos_rotation = 1.0f;
    float  x            = rectangle.x;
    float  y            = rectangle.y;
    float  dx           = -origin.x;
    float  dy           = -origin.y;

    if (rotation != 0.0f) {
        float radians = rotation * DEG2RAD;

        sin_rotation = sinf(radians);
        cos_rotation = cosf(radians);
    }

    vec2_t corner[4];
    corner[0].x = x + dx * cos_rotation - dy * sin_rotation;

    corner[0].y = y + dx * sin_rotation + dy * cos_rotation;

    corner[1].x = x + dx * cos_rotation -
                  (dy + rectangle.height) * sin_rotation;

    corner[1].y = y + dx * sin_rotation +
                  (dy + rectangle.height) * cos_rotation;

    corner[2].x = x + (dx + rectangle.width) * cos_rotation -
                  (dy + rectangle.height) * sin_rotation;

    corner[2].y = y + (dx + rectangle.width) * sin_rotation +
                  (dy + rectangle.height) * cos_rotation;

    corner[3].x = x + (dx + rectangle.width) * cos_rotation - dy * sin_rotation;

    corner[3].y = y + (dx + rectangle.width) * sin_rotation + dy * cos_rotation;

    graphic_begin(GRAPHIC_DRAW_QUADS);
    color_apply(color);
    graphic_vertex2f(corner[0].x, corner[0].y);
    graphic_vertex2f(corner[1].x, corner[1].y);
    graphic_vertex2f(corner[2].x, corner[2].y);
    graphic_vertex2f(corner[3].x, corner[3].y);
    graphic_end();
}

void graphic_draw_rectangle_gradient_v(int             x,
                                       int             y,
                                       int             width,
                                       int             height,
                                       graphic_color_t top,
                                       graphic_color_t bottom)
{
    rect_t rectangle = { (float)x, (float)y, (float)width, (float)height };

    graphic_draw_rectangle_gradient_ex(rectangle, top, bottom, bottom, top);
}

void graphic_draw_rectangle_gradient_h(int             x,
                                       int             y,
                                       int             width,
                                       int             height,
                                       graphic_color_t left,
                                       graphic_color_t right)
{
    rect_t rectangle = { (float)x, (float)y, (float)width, (float)height };

    graphic_draw_rectangle_gradient_ex(rectangle, left, left, right, right);
}

void graphic_draw_rectangle_lines(int             x,
                                  int             y,
                                  int             width,
                                  int             height,
                                  graphic_color_t color)
{
    float left   = (float)x;
    float top    = (float)y;
    float right  = (float)(x + width);
    float bottom = (float)(y + height);

    graphic_begin(GRAPHIC_DRAW_LINES);
    color_apply(color);

    graphic_vertex2f(left, top);
    graphic_vertex2f(right, top);

    graphic_vertex2f(right, top);
    graphic_vertex2f(right, bottom);

    graphic_vertex2f(right, bottom);
    graphic_vertex2f(left, bottom);

    graphic_vertex2f(left, bottom);
    graphic_vertex2f(left, top);

    graphic_end();
}

/*
 * A thick border is four filled bars, not four lines, for the same
 * reason graphic_draw_line_ex() is a quad: wide lines are not
 * portable. The bars are cut so the corners are not drawn twice,
 * which would be visible with a translucent colour.
 */
void graphic_draw_rectangle_lines_ex(rect_t          rectangle,
                                     float           line_thick,
                                     graphic_color_t color)
{
    if (line_thick <= 0.0f)
        return;

    if (line_thick > rectangle.width / 2.0f)
        line_thick = rectangle.width / 2.0f;
    if (line_thick > rectangle.height / 2.0f)
        line_thick = rectangle.height / 2.0f;

    rect_t top = (rect_t){ rectangle.x, rectangle.y, rectangle.width, line_thick };

    rect_t bottom = (rect_t){ rectangle.x,
                              rectangle.y + rectangle.height - line_thick,
                              rectangle.width,
                              line_thick };

    rect_t left = (rect_t){ rectangle.x,
                            rectangle.y + line_thick,
                            line_thick,
                            rectangle.height - line_thick * 2.0f };

    rect_t right = (rect_t){ rectangle.x + rectangle.width - line_thick,
                             rectangle.y + line_thick,
                             line_thick,
                             rectangle.height - line_thick * 2.0f };

    graphic_draw_rectangle_rec(top, color);
    graphic_draw_rectangle_rec(bottom, color);
    graphic_draw_rectangle_rec(left, color);
    graphic_draw_rectangle_rec(right, color);
}

/*
 * The corners in the order an outline is walked, clockwise on screen
 * from the top left because y grows downwards, and the angle each
 * corner's arc starts at.
 */
static const int corner_walk[4] = {
    GRAPHIC_CORNER_TOP_LEFT,
    GRAPHIC_CORNER_TOP_RIGHT,
    GRAPHIC_CORNER_BOTTOM_RIGHT,
    GRAPHIC_CORNER_BOTTOM_LEFT
};

static const float corner_start_angle[4] = { 180.0f, 270.0f, 0.0f, 90.0f };

/*
 * Radius of every corner, in walk order. Each is capped at half the
 * shorter side, so neighbouring arcs can meet but never cross. Returns
 * whether any corner is rounded at all.
 */
static bool corner_radii_get(rect_t      rectangle,
                             const float roundness[4],
                             float       radius[4])
{
    float shortest = (rectangle.width < rectangle.height) ?
                      rectangle.width : rectangle.height;
    bool rounded   = false;

    for (int i = 0; i < 4; i++) {
        float value = (roundness != NULL) ? roundness[corner_walk[i]] : 0.0f;

        /* written this way round so a NaN counts as square too */
        if (!(value > 0.0f))
            value = 0.0f;
        if (value > 1.0f)
            value = 1.0f;

        radius[i] = shortest * value / 2.0f;
        if (radius[i] > 0.0f)
            rounded = true;
    }

    return rounded;
}

/* the centre of a corner's arc, which for a square corner is the corner */
static vec2_t corner_center_get(rect_t rectangle, int walk, float radius)
{
    float left   = rectangle.x + radius;
    float right  = rectangle.x + rectangle.width - radius;
    float top    = rectangle.y + radius;
    float bottom = rectangle.y + rectangle.height - radius;

    switch (walk) {
        case 0:
            return (vec2_t){  left,    top };
        case 1:
            return (vec2_t){ right,    top };
        case 2:
            return (vec2_t){ right, bottom };
        default:
            return (vec2_t){  left, bottom };
    }
}

/*
 * Drawn as one fan from the middle over the outline. The corners now
 * differ, so the old cross of three rectangles no longer fits. The
 * shape is still convex, so a fan still covers it exactly, and it
 * lands in one run of triangles. A square corner contributes its one
 * point to the outline.
 */
void graphic_draw_rectangle_rounded(rect_t          rectangle,
                                    const float     roundness[4],
                                    int             segments,
                                    graphic_color_t color)
{
    float  radius[4];
    bool   started  = false;
    vec2_t first    = { 0.0f, 0.0f };
    vec2_t previous = { 0.0f, 0.0f };

    if (rectangle.width < 1.0f ||
        rectangle.height < 1.0f ||
        !corner_radii_get(rectangle, roundness, radius)) {
        graphic_draw_rectangle_rec(rectangle, color);
        return;
    }

    vec2_t middle = (vec2_t){ rectangle.x + rectangle.width / 2.0f,
                              rectangle.y + rectangle.height / 2.0f };

    graphic_begin(GRAPHIC_DRAW_TRIANGLES);
    color_apply(color);

    for (int corner = 0; corner < 4; corner++) {
        vec2_t center = corner_center_get(rectangle, corner, radius[corner]);
        int    steps  = 0;

        if (radius[corner] > 0.0f)
            steps = (segments < 4) ?
                    circle_segments_get(radius[corner], 90.0f) :
                    segments;

        /*
         * Step 0 of each corner also closes the straight edge from
         * the previous corner. Where two full arcs meet that
         * triangle is degenerate, which costs nothing.
         */
        for (int step = 0; step <= steps; step++) {
            vec2_t point = center;

            if (steps > 0)
                point = point_on_arc(center,
                                     radius[corner],
                                     radius[corner],
                                     corner_start_angle[corner] +
                                     90.0f * (float)step / (float)steps);

            if (started) {
                graphic_vertex2f(middle.x, middle.y);
                graphic_vertex2f(previous.x, previous.y);
                graphic_vertex2f(point.x, point.y);
            } else {
                first   = point;
                started = true;
            }

            previous = point;
        }
    }

    /* the left edge, back to where the walk began */
    graphic_vertex2f(middle.x, middle.y);
    graphic_vertex2f(previous.x, previous.y);
    graphic_vertex2f(first.x, first.y);

    graphic_end();
}

/*
 * The outline is centred on the edge, as graphic_draw_line_ex() centres
 * a line. A rounded corner is a ring along the arc and its edges stop
 * where the arc begins. At a square corner the horizontal edge runs
 * half the thickness past the corner and the vertical one stops half
 * the thickness short of it. That fills the corner square exactly
 * once, with no notch outside and no overlap to show through a
 * translucent colour.
 */
void graphic_draw_rectangle_rounded_lines_ex(rect_t          rectangle,
                                             const float     roundness[4],
                                             int             segments,
                                             float           line_thick,
                                             graphic_color_t color)
{
    float radius[4];
    float along_h[4]; /* where the horizontal edge stops */
    float along_v[4]; /* and the vertical one */

    if (line_thick <= 0.0f)
        return;

    if (!corner_radii_get(rectangle, roundness, radius)) {
        graphic_draw_rectangle_lines_ex(rectangle, line_thick, color);
        return;
    }

    float half = line_thick / 2.0f;

    /* inset from the corner, in walk order */
    for (int corner = 0; corner < 4; corner++) {
        along_h[corner] = (radius[corner] > 0.0f) ? radius[corner] : -half;
        along_v[corner] = (radius[corner] > 0.0f) ? radius[corner] : half;
    }

    /* four straight edges */
    vec2_t a = (vec2_t){ rectangle.x + along_h[0], rectangle.y };
    vec2_t b = (vec2_t){ rectangle.x + rectangle.width - along_h[1], rectangle.y };
    graphic_draw_line_ex(a, b, line_thick, color);

    a = (vec2_t){ rectangle.x + rectangle.width, rectangle.y + along_v[1] };
    b = (vec2_t){ rectangle.x + rectangle.width,
                  rectangle.y + rectangle.height - along_v[2] };
    graphic_draw_line_ex(a, b, line_thick, color);

    a = (vec2_t){ rectangle.x + along_h[3], rectangle.y + rectangle.height };
    b = (vec2_t){ rectangle.x + rectangle.width - along_h[2],
                  rectangle.y + rectangle.height };
    graphic_draw_line_ex(a, b, line_thick, color);

    a = (vec2_t){ rectangle.x, rectangle.y + along_v[0] };
    b = (vec2_t){ rectangle.x, rectangle.y + rectangle.height - along_v[3] };
    graphic_draw_line_ex(a, b, line_thick, color);

    /* and the rounded corners, drawn as rings so they carry thickness */
    for (int corner = 0; corner < 4; corner++) {
        int steps;

        if (radius[corner] <= 0.0f)
            continue;

        steps = (segments < 4) ? circle_segments_get(radius[corner], 90.0f) :
                                 segments;

        graphic_draw_ring(corner_center_get(rectangle, corner, radius[corner]),
                          radius[corner] - half,
                          radius[corner] + half,
                          corner_start_angle[corner],
                          corner_start_angle[corner] + 90.0f,
                          steps,
                          color);
    }
}

/*
 * The colour at a point, blended from the four corners of the
 * rectangle the way a bilinear sample would.
 */
static graphic_color_t corner_blend(rect_t          rectangle,
                                    vec2_t          point,
                                    graphic_color_t top_left,
                                    graphic_color_t bottom_left,
                                    graphic_color_t bottom_right,
                                    graphic_color_t top_right)
{
    float u = (rectangle.width > 0.0f) ?
              (point.x - rectangle.x) / rectangle.width :
              0.0f;
    float v = (rectangle.height > 0.0f) ?
              (point.y - rectangle.y) / rectangle.height :
              0.0f;
    graphic_color_t top    = graphic_color_lerp(top_left, top_right, u);
    graphic_color_t bottom = graphic_color_lerp(bottom_left, bottom_right, u);

    return graphic_color_lerp(top, bottom, v);
}

static void blended_vertex(rect_t          rectangle,
                           vec2_t          point,
                           graphic_color_t top_left,
                           graphic_color_t bottom_left,
                           graphic_color_t bottom_right,
                           graphic_color_t top_right)
{
    color_apply(corner_blend(rectangle,
                             point,
                             top_left,
                             bottom_left,
                             bottom_right,
                             top_right));
    graphic_vertex2f(point.x, point.y);
}

void graphic_draw_rectangle_rounded_gradient(rect_t          rectangle,
                                             float           roundness,
                                             int             segments,
                                             graphic_color_t top_left,
                                             graphic_color_t bottom_left,
                                             graphic_color_t bottom_right,
                                             graphic_color_t top_right)
{
    if (rectangle.width < 1.0f || rectangle.height < 1.0f)
        return;

    if (roundness < 0.0f)
        roundness = 0.0f;

    if (roundness > 1.0f)
        roundness = 1.0f;

    float shortest = (rectangle.width < rectangle.height) ?
                      rectangle.width : rectangle.height;
    float radius   = shortest * roundness / 2.0f;

    if (segments < 4)
        segments = circle_segments_get(radius, 90.0f);

    vec2_t middle = (vec2_t){ rectangle.x + rectangle.width / 2.0f,
                              rectangle.y + rectangle.height / 2.0f };

    /* the four arc centres, in the order the outline is walked */
    /* clockwise on screen, because y grows downwards           */
    vec2_t centers[4];
    centers[0] = (vec2_t){ rectangle.x + radius, rectangle.y + radius };
    centers[1] = (vec2_t){ rectangle.x + rectangle.width - radius,
                           rectangle.y + radius };
    centers[2] = (vec2_t){ rectangle.x + rectangle.width - radius,
                           rectangle.y + rectangle.height - radius };
    centers[3] = (vec2_t){ rectangle.x + radius,
                           rectangle.y + rectangle.height - radius };

    graphic_begin(GRAPHIC_DRAW_TRIANGLES);

    /*
     * One triangle per step of the outline, all sharing the middle.
     * Interpolating three exactly correct colours over a triangle
     * reproduces a horizontal or vertical gradient exactly; only the
     * cross term of four differing corners is approximated, and the
     * fan approximates it symmetrically.
     */
    float start_angle[4] = { 180.0f, 270.0f, 0.0f, 90.0f };
    for (int corner = 0; corner < 4; corner++) {
        for (int step = 0; step < segments; step++) {
            float from = (start_angle[corner] +
                          90.0f * (float)step / (float)segments) * DEG2RAD;

            float to   = (start_angle[corner] +
                          90.0f * (float)(step + 1) / (float)segments) * DEG2RAD;

            vec2_t a   = { centers[corner].x + cosf(from) * radius,
                           centers[corner].y + sinf(from) * radius };

            vec2_t b   = { centers[corner].x + cosf(to) * radius,
                           centers[corner].y + sinf(to) * radius };

            blended_vertex(rectangle,
                           middle,
                           top_left,
                           bottom_left,
                           bottom_right,
                           top_right);

            blended_vertex(rectangle,
                           a,
                           top_left,
                           bottom_left,
                           bottom_right,
                           top_right);

            blended_vertex(rectangle,
                           b,
                           top_left,
                           bottom_left,
                           bottom_right,
                           top_right);
        }

        /*
         * The straight edge from this arc's end to the next
         * arc's start. With roundness at one the two meet and
         * the triangle is degenerate, which costs nothing.
         */
        {
            int   next  = (corner + 1) % 4;
            float end   = (start_angle[corner] + 90.0f) * DEG2RAD;
            float begin = start_angle[next] * DEG2RAD;

            vec2_t a    = { centers[corner].x + cosf(end) * radius,
                            centers[corner].y + sinf(end) * radius };

            vec2_t b    = { centers[next].x + cosf(begin) * radius,
                            centers[next].y + sinf(begin) * radius };

            blended_vertex(rectangle,
                           middle,
                           top_left,
                           bottom_left,
                           bottom_right,
                           top_right);

            blended_vertex(rectangle,
                           a,
                           top_left,
                           bottom_left,
                           bottom_right,
                           top_right);

            blended_vertex(rectangle,
                           b,
                           top_left,
                           bottom_left,
                           bottom_right,
                           top_right);
        }
    }

    graphic_end();
}

void graphic_draw_rectangle_rounded_lines(rect_t          rectangle,
                                          const float     roundness[4],
                                          int             segments,
                                          graphic_color_t color)
{
    graphic_draw_rectangle_rounded_lines_ex(rectangle,
                                            roundness,
                                            segments,
                                            1.0f,
                                            color);
}

/* ------------------------------------------------------------------ */
/* triangles and polygons                                             */
/* ------------------------------------------------------------------ */

void graphic_draw_triangle(vec2_t          v1,
                           vec2_t          v2,
                           vec2_t          v3,
                           graphic_color_t color)
{
    graphic_begin(GRAPHIC_DRAW_TRIANGLES);
    color_apply(color);
    graphic_vertex2f(v1.x, v1.y);
    graphic_vertex2f(v2.x, v2.y);
    graphic_vertex2f(v3.x, v3.y);
    graphic_end();
}

void graphic_draw_triangle_lines(vec2_t          v1,
                                 vec2_t          v2,
                                 vec2_t          v3,
                                 graphic_color_t color)
{
    graphic_begin(GRAPHIC_DRAW_LINES);
    color_apply(color);
    graphic_vertex2f(v1.x, v1.y);
    graphic_vertex2f(v2.x, v2.y);
    graphic_vertex2f(v2.x, v2.y);
    graphic_vertex2f(v3.x, v3.y);
    graphic_vertex2f(v3.x, v3.y);
    graphic_vertex2f(v1.x, v1.y);
    graphic_end();
}

void graphic_draw_triangle_fan(const vec2_t   *points,
                               int             point_count,
                               graphic_color_t color)
{
    if (points == NULL || point_count < 3)
        return;

    graphic_begin(GRAPHIC_DRAW_TRIANGLES);
    color_apply(color);

    for (int i = 1; i < point_count - 1; i++) {
        graphic_vertex2f(points[0].x, points[0].y);
        graphic_vertex2f(points[i].x, points[i].y);
        graphic_vertex2f(points[i + 1].x, points[i + 1].y);
    }

    graphic_end();
}

void graphic_draw_triangle_strip(const vec2_t   *points,
                                 int             point_count,
                                 graphic_color_t color)
{
    if (points == NULL || point_count < 3)
        return;

    graphic_begin(GRAPHIC_DRAW_TRIANGLES);
    color_apply(color);

    /* every other triangle is reversed, as a real strip would be */
    for (int i = 2; i < point_count; i++) {
        if (i % 2 == 0) {
            graphic_vertex2f(points[i].x, points[i].y);
            graphic_vertex2f(points[i - 2].x, points[i - 2].y);
            graphic_vertex2f(points[i - 1].x, points[i - 1].y);
        } else {
            graphic_vertex2f(points[i].x, points[i].y);
            graphic_vertex2f(points[i - 1].x, points[i - 1].y);
            graphic_vertex2f(points[i - 2].x, points[i - 2].y);
        }
    }

    graphic_end();
}

void graphic_draw_poly(vec2_t          center,
                       int             sides,
                       float           radius,
                       float           rotation,
                       graphic_color_t color)
{
    if (sides < 3)
        return;

    float step = 360.0f / (float)sides;
    float angle = rotation;

    graphic_begin(GRAPHIC_DRAW_TRIANGLES);
    color_apply(color);

    for (int i = 0; i < sides; i++) {
        vec2_t a = point_on_arc(center, radius, radius, angle);
        vec2_t b = point_on_arc(center, radius, radius, angle + step);

        graphic_vertex2f(center.x, center.y);
        graphic_vertex2f(a.x, a.y);
        graphic_vertex2f(b.x, b.y);

        angle += step;
    }

    graphic_end();
}

void graphic_draw_poly_lines(vec2_t          center,
                             int             sides,
                             float           radius,
                             float           rotation,
                             graphic_color_t color)
{
    if (sides < 3)
        return;

    float step = 360.0f / (float)sides;
    float angle = rotation;

    graphic_begin(GRAPHIC_DRAW_LINES);
    color_apply(color);

    for (int i = 0; i < sides; i++) {
        vec2_t a = point_on_arc(center, radius, radius, angle);
        vec2_t b = point_on_arc(center, radius, radius, angle + step);

        graphic_vertex2f(a.x, a.y);
        graphic_vertex2f(b.x, b.y);

        angle += step;
    }

    graphic_end();
}

void graphic_draw_poly_lines_ex(vec2_t          center,
                                int             sides,
                                float           radius,
                                float           rotation,
                                float           line_thick,
                                graphic_color_t color)
{
    if (sides < 3 || line_thick <= 0.0f)
        return;

    float step  = 360.0f / (float)sides;
    float angle = rotation;

    for (int i = 0; i < sides; i++) {
        vec2_t a = point_on_arc(center, radius, radius, angle);
        vec2_t b = point_on_arc(center, radius, radius, angle + step);

        graphic_draw_line_ex(a, b, line_thick, color);
        angle += step;
    }
}