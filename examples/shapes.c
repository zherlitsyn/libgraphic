#include <math.h>
#include <stdio.h>

#include "graphic.h"

/*
 * A pass over the shape API, one row per family of shapes. It grows
 * along with the library.
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

    vec2_t strip[5];
    for (int i = 0; i < 5; i++) {
        strip[i].x = 490.0f + (float)i * 22.0f;
        strip[i].y = y + ((i % 2 == 0) ? -12.0f : 12.0f);
    }

    graphic_draw_line_strip(strip, 5, red);
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

int main(void)
{
    graphic_window_flags_set(GRAPHIC_WINDOW_RESIZABLE |
                             GRAPHIC_WINDOW_VSYNC     |
                             GRAPHIC_WINDOW_VALIDATION);

    if (!graphic_window(960, 480, "graphic - shapes"))
        return 1;

    graphic_fps_target_set(60);

    while (!graphic_window_should_close()) {
        float time = (float)graphic_time_get();

        graphic_drawing_begin();

        graphic_color_t background = { 64, 64, 64, 255 };
        graphic_background_clear(background);

        row_lines(60.0f);
        row_rectangles(250.0f, time);
        row_polygons(350.0f);

        graphic_drawing_end();
    }

    graphic_window_close();

    return 0;
}