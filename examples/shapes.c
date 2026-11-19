#include <math.h>
#include <stdio.h>

#include "graphic.h"

/*
* A pass over the shape API. Every family is drawn once, and the
* bottom strip is interactive so the intersection tests are exercised
* against the pointer rather than only compiled.
 */

static const graphic_color_t white = { 245, 245, 245, 255 };
static const graphic_color_t grey  = { 128, 128, 128, 255 };
static const graphic_color_t red   = { 220,  70,  70, 255 };
static const graphic_color_t green = {  70, 200, 120, 255 };
static const graphic_color_t blue  = {  80, 130, 240, 255 };
static const graphic_color_t amber = { 240, 180,  60, 255 };

static void row_lines(float y)
{
    graphic_draw_pixel(30, (int)y, white);

    graphic_draw_line(50, (int)y - 10, 130, (int)y + 10, grey);

    graphic_draw_line_ex((vec2_t){ 150.0f, y - 10.0f },
                         (vec2_t){ 230.0f, y + 10.0f },
                         6.0f,
                         blue);

    graphic_draw_line_dashed((vec2_t){ 250.0f, y },
                             (vec2_t){ 350.0f, y },
                             8,
                             6,
                             amber);

    graphic_draw_line_bezier((vec2_t){ 370.0f, y - 15.0f },
                             (vec2_t){ 460.0f, y + 15.0f },
                             3.0f,
                             green);

    vec2_t strip[5];
    for (int i = 0; i < 5; i++) {
        strip[i].x = 490.0f + (float)i * 22.0f;
        strip[i].y = y + ((i % 2 == 0) ? -12.0f : 12.0f);
    }

    graphic_draw_line_strip(strip, 5, red);
}

static void row_circles(float y)
{
    graphic_draw_circle(60, (int)y, 26.0f, red);

    graphic_draw_circle_lines(140, (int)y, 26.0f, white);

    graphic_draw_circle_gradient((vec2_t){ 220.0f, y },
                                 28.0f,
                                 amber,
                                 (graphic_color_t){ 240, 180, 60, 0 });

    graphic_draw_circle_sector((vec2_t){ 300.0f, y },
                               30.0f,
                               0.0f,
                               120.0f,
                               0,
                               green);

    graphic_draw_circle_sector_lines((vec2_t){ 380.0f, y },
                                     30.0f,
                                     30.0f,
                                     300.0f,
                                     0,
                                     blue);

    graphic_draw_ellipse(470, (int)y, 40.0f, 22.0f, grey);

    graphic_draw_ellipse_lines(560, (int)y, 40.0f, 22.0f, white);

    graphic_draw_ring((vec2_t){ 650.0f, y },
                      16.0f,
                      30.0f,
                      0.0f,
                      270.0f,
                      0,
                      amber);

    graphic_draw_ring_lines((vec2_t){ 730.0f, y },
                            16.0f,
                            30.0f,
                            45.0f,
                            315.0f,
                            0,
                            white);
}

static void row_rectangles(float y, float time)
{
    graphic_draw_rectangle(30, (int)y - 25, 70, 50, red);

    graphic_draw_rectangle_lines(120, (int)y - 25, 70, 50, white);

    graphic_draw_rectangle_lines_ex((rect_t){ 210.0f, y - 25.0f, 70.0f, 50.0f },
                                    5.0f,
                                    blue);

    graphic_draw_rectangle_gradient_v(300, (int)y - 25, 70, 50, amber, red);

    graphic_draw_rectangle_gradient_h(390, (int)y - 25, 70, 50, green, blue);

    /* top left, bottom left, bottom right, top right */
    graphic_draw_rectangle_rounded((rect_t){ 480.0f, y - 25.0f, 80.0f, 50.0f },
                                   (const float[4]){ 0.5f, 0.5f, 0.5f, 0.5f },
                                   0,
                                   grey);

    graphic_draw_rectangle_rounded_lines_ex((rect_t){ 580.0f,
                                                      y - 25.0f,
                                                      80.0f,
                                                      50.0f },
                                            (const float[4]){ 1.0f,
                                                              0.0f,
                                                              1.0f,
                                                              0.0f },
                                            0,
                                            3.0f,
                                            white);

    /* the pivot is the centre, so it spins in place */
    graphic_draw_rectangle_pro((rect_t){ 720.0f, y, 60.0f, 40.0f },
                               (vec2_t){ 30.0f, 20.0f },
                               time * 40.0f,
                               green);
}

static void row_polygons(float y)
{
    graphic_draw_triangle((vec2_t){ 60.0f, y - 25.0f },
                          (vec2_t){ 30.0f, y + 25.0f },
                          (vec2_t){ 90.0f, y + 25.0f },
                          red);

    graphic_draw_triangle_lines((vec2_t){ 150.0f, y - 25.0f },
                                (vec2_t){ 120.0f, y + 25.0f },
                                (vec2_t){ 180.0f, y + 25.0f },
                                white);

    vec2_t fan[6];
    fan[0] = (vec2_t){ 250.0f, y };
    for (int i = 1; i < 6; i++) {
        float angle = (float)(i - 1) * 45.0f * DEG2RAD;

        fan[i].x = 250.0f + cosf(angle) * 35.0f;
        fan[i].y = y + sinf(angle) * 35.0f;
    }
    graphic_draw_triangle_fan(fan, 6, blue);

    vec2_t strip[6];
    for (int i = 0; i < 6; i++) {
        strip[i].x = 340.0f + (float)i * 18.0f;
        strip[i].y = y + ((i % 2 == 0) ? -25.0f : 25.0f);
    }
    graphic_draw_triangle_strip(strip, 6, green);

    graphic_draw_poly((vec2_t){ 500.0f, y }, 6, 32.0f, 0.0f, amber);

    graphic_draw_poly_lines((vec2_t){ 580.0f, y }, 5, 32.0f, 20.0f, white);

    graphic_draw_poly_lines_ex((vec2_t){ 660.0f, y },
                               8,
                               32.0f,
                               0.0f,
                               4.0f,
                               grey);
}

static void row_splines(float y)
{
    vec2_t points[7];

    for (int i = 0; i < 7; i++) {
        points[i].x = 40.0f + (float)i * 60.0f;
        points[i].y = y + ((i % 2 == 0) ? -20.0f : 20.0f);
    }

    graphic_draw_spline_linear(points, 7, 2.0f, grey);
    graphic_draw_spline_catmull_rom(points, 7, 3.0f, amber);
    graphic_draw_spline_basis(points, 7, 3.0f, blue);

    for (int i = 0; i < 7; i++)
        graphic_draw_circle_v(points[i], 3.0f, white);
}

/* the pointer against a rectangle, a circle and a triangle */
static void row_collisions(float y)
{
    rect_t box = { 60.0f, y - 30.0f, 120.0f, 60.0f };
    vec2_t circle = { 280.0f, y };
    vec2_t triangle[3] = {
        { 420.0f, y - 30.0f },
        { 380.0f, y + 30.0f },
        { 470.0f, y + 30.0f },
    };
    vec2_t pointer = graphic_mouse_position_get();

    graphic_draw_rectangle_rec(box,
                               graphic_point_in_rect(pointer, box) ?
                               green : grey);

    graphic_draw_circle_v(circle,
                          35.0f,
                          graphic_point_in_circle(pointer, circle, 35.0f) ?
                          green : grey);

    graphic_draw_triangle(triangle[0],
                          triangle[1],
                          triangle[2],
                          graphic_point_in_triangle(pointer,
                                                    triangle[0],
                                                    triangle[1],
                                                    triangle[2]) ? green : grey);

    /* a segment the pointer drags a second segment against */
    vec2_t fixed_start  = { 540.0f, y - 30.0f };
    vec2_t fixed_end    = { 700.0f, y + 30.0f };
    vec2_t moving_start = { 540.0f, y + 30.0f };
    vec2_t hit;
    bool crossing = graphic_line_collide_line(fixed_start,
                                              fixed_end,
                                              moving_start,
                                              pointer,
                                              &hit);

    graphic_draw_line_ex(fixed_start, fixed_end, 3.0f, white);
    graphic_draw_line_ex(moving_start, pointer, 3.0f, crossing ? green : grey);

    if (crossing)
        graphic_draw_circle_v(hit, 6.0f, red);
}

int main(void)
{
    graphic_window_flags_set(GRAPHIC_WINDOW_RESIZABLE |
                             GRAPHIC_WINDOW_VSYNC     |
                             GRAPHIC_WINDOW_VALIDATION);

    if (!graphic_window(1280, 720, "graphic - shapes"))
        return 1;

    graphic_fps_target_set(60);

    printf("move the pointer over the bottom row to exercise the intersection tests\n");

    while (!graphic_window_should_close()) {
        float time = (float)graphic_time_get();

        graphic_drawing_begin();

        graphic_color_t background = { 64, 64, 64, 255 };
        graphic_background_clear(background);

        row_lines(60.0f);
        row_circles(150.0f);
        row_rectangles(250.0f, time);
        row_polygons(350.0f);
        row_splines(470.0f);
        row_collisions(600.0f);

        graphic_drawing_end();
    }

    graphic_window_close();

    return 0;
}