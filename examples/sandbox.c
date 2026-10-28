#include <math.h>

#include "graphic.h"

static void quad_draw(float           x,
                      float           y,
                      float           size,
                      graphic_color_t color)
{
    graphic_begin(GRAPHIC_DRAW_QUADS);
    graphic_color4ub(color.r, color.g, color.b, color.a);
    graphic_vertex2f(x, y);
    graphic_vertex2f(x, y + size);
    graphic_vertex2f(x + size, y + size);
    graphic_vertex2f(x + size, y);
    graphic_end();
}

static void grid_draw(int width,
                      int height,
                      int step)
{
    graphic_begin(GRAPHIC_DRAW_LINES);
    graphic_color4ub(128, 128, 128, 255);

    for (int i = 0; i <= width; i += step) {
        graphic_vertex2f((float)i, 0.0f);
        graphic_vertex2f((float)i, (float)height);
    }

    for (int i = 0; i <= height; i += step) {
        graphic_vertex2f(0.0f, (float)i);
        graphic_vertex2f((float)width, (float)i);
    }

    graphic_end();
}

/* a wheel of triangles around a centre, one colour per slice */
static void wheel_draw(float x,
                       float y,
                       float radius,
                       int   slices)
{
    graphic_begin(GRAPHIC_DRAW_TRIANGLES);

    for (int i = 0; i < slices; i++) {
        float a0 = 2.0f * MATH_PI * (float)i / (float)slices;
        float a1 = 2.0f * MATH_PI * (float)(i + 1) / (float)slices;

        graphic_color4f(0.5f + 0.5f * cosf(a0), 0.5f + 0.5f * sinf(a0),
                0.8f, 1.0f);
        graphic_vertex2f(x, y);
        graphic_vertex2f(x + radius * cosf(a0), y + radius * sinf(a0));
        graphic_vertex2f(x + radius * cosf(a1), y + radius * sinf(a1));
    }

    graphic_end();
}

int main(void)
{
    graphic_window_flags_set(GRAPHIC_WINDOW_RESIZABLE |
                             GRAPHIC_WINDOW_VSYNC     |
                             GRAPHIC_WINDOW_VALIDATION);

    if (!graphic_window(960, 480, "graphic - sandbox"))
        return 1;

    float angle = 0.0f;
    while (!graphic_window_should_close()) {
        angle += 45.0f * graphic_frame_time_get();

        graphic_drawing_begin();

        graphic_color_t background = { 64, 64, 64, 255 };
        graphic_background_clear(background);

        grid_draw(graphic_render_width_get(),
                  graphic_render_height_get(),
                  40);

        /* a square turning about its own centre */
        graphic_matrix_push();
        graphic_translatef(240.0f, 270.0f, 0.0f);
        graphic_rotatef(angle, 0.0f, 0.0f, 1.0f);

        graphic_color_t square = { 255, 0, 0, 255 };
        quad_draw(-80.0f, -80.0f, 160.0f, square);

        graphic_matrix_pop();

        wheel_draw(640.0f, 270.0f, 140.0f, 24);

        graphic_drawing_end();
    }

    graphic_window_close();

    return 0;
}