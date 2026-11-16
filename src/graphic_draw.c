#include <math.h>
#include <stddef.h>

#include "graphic_internal.h"

/*
 * 2D shapes on top of the immediate mode calls. Not one line of this
 * file knows about Vulkan: everything becomes graphic_begin, vertices
 * and graphic_end, so shapes share the frame batch with 3D geometry
 * and cost no extra submit.
 */

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