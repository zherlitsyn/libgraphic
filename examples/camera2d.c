#include <stdio.h>

#include "graphic.h"

/*
 * 2D camera, scissor and blend modes in one scene.
 *
 * Left half: a world drawn through a 2D camera you can pan, zoom and
 * rotate. Right half: the same geometry with no camera, clipped by a
 * scissor rectangle, drawn once per blend mode so the difference is
 * visible.
 */

static void quad_draw(float x,
                      float y,
                      float width,
                      float height,
                      graphic_color_t color)
{
    graphic_begin(GRAPHIC_DRAW_QUADS);
    graphic_color4ub(color.r, color.g, color.b, color.a);
    graphic_vertex2f(x, y + height);
    graphic_vertex2f(x + width, y + height);
    graphic_vertex2f(x + width, y);
    graphic_vertex2f(x, y);
    graphic_end();
}

static void grid_draw(int   cells,
                      float spacing)
{
    float extent = (float)cells * spacing;

    graphic_begin(GRAPHIC_DRAW_LINES);
    graphic_color4ub(70, 74, 90, 255);

    for (int i = 0; i <= cells; i++) {
        float offset = (float)i * spacing;

        graphic_vertex2f(offset, 0.0f);
        graphic_vertex2f(offset, extent);
        graphic_vertex2f(0.0f, offset);
        graphic_vertex2f(extent, offset);
    }

    graphic_end();
}

static void world_draw(void)
{
    graphic_color_t red   = { 220, 70, 70, 255 };
    graphic_color_t green = { 70, 200, 120, 255 };
    graphic_color_t blue  = { 80, 130, 240, 255 };

    grid_draw(20, 40.0f);

    quad_draw(100.0f, 100.0f, 120.0f, 80.0f, red);
    quad_draw(300.0f, 200.0f, 90.0f, 140.0f, green);
    quad_draw(180.0f, 380.0f, 200.0f, 60.0f, blue);
}

/* three overlapping squares, so the blend maths is actually exercised */
static void blend_sample_draw(float x, float y, int blend_mode)
{
    static const graphic_color_t colors[] = {
        { 255, 64, 64, 128 },
        { 64, 255, 64, 128 },
        { 64, 64, 255, 128 },
    };

    graphic_mode_blend_begin(blend_mode);

    for (int i = 0; i < 3; i++) {
        float offset_x = x + (float)i * 22.0f;
        float offset_y = y + ((i == 1) ? 24.0f : 0.0f);

        quad_draw(offset_x, offset_y, 50.0f, 50.0f, colors[i]);
    }

    graphic_mode_blend_end();
}

int main(void)
{
    graphic_camera2d_t camera;
    camera.offset   = (vec2_t){ 240.0f, 270.0f };
    camera.target   = (vec2_t){ 240.0f, 270.0f };
    camera.rotation = 0.0f;
    camera.zoom     = 1.0f;

    graphic_window_flags_set(GRAPHIC_WINDOW_RESIZABLE |
                             GRAPHIC_WINDOW_VSYNC     |
                             GRAPHIC_WINDOW_VALIDATION);

    if (!graphic_window(960, 480, "graphic - camera 2d"))
        return 1;

    graphic_fps_target_set(60);

    printf("wasd or drag with the left button pans, wheel zooms, "
           "q and e rotate, space cycles the blend mode\n");

    int blend_mode = GRAPHIC_BLEND_ALPHA;

    while (!graphic_window_should_close()) {
        float delta      = graphic_frame_time_get();
        float wheel      = graphic_mouse_wheel_move_get();
        int   half_width = graphic_render_width_get() / 2;

        /* pan */
        if (graphic_is_key_down(GRAPHIC_KEY_W))
            camera.target.y -= 200.0f * delta;

        if (graphic_is_key_down(GRAPHIC_KEY_S))
            camera.target.y += 200.0f * delta;

        if (graphic_is_key_down(GRAPHIC_KEY_A))
            camera.target.x -= 200.0f * delta;

        if (graphic_is_key_down(GRAPHIC_KEY_D))
            camera.target.x += 200.0f * delta;

        if (graphic_is_mouse_button_down(GRAPHIC_MOUSE_BUTTON_LEFT)) {
            vec2_t drag = graphic_mouse_delta_get();

            camera.target = vec2_subtract(camera.target,
                                          vec2_scale(drag, 1.0f / camera.zoom));
        }

        if (graphic_is_key_down(GRAPHIC_KEY_Q))
            camera.rotation -= 45.0f * delta;

        if (graphic_is_key_down(GRAPHIC_KEY_E))
            camera.rotation += 45.0f * delta;

        /* zoom towards the pointer, the way a map does */
        if (wheel != 0.0f) {
            vec2_t pointer = graphic_mouse_position_get();
            vec2_t before  = graphic_screen_to_world2d_get(pointer, camera);
            vec2_t after;

            camera.zoom += wheel * 0.15f * camera.zoom;

            if (camera.zoom < 0.1f)
                camera.zoom = 0.1f;

            if (camera.zoom > 8.0f)
                camera.zoom = 8.0f;

            after = graphic_screen_to_world2d_get(pointer, camera);
            camera.target = vec2_add(camera.target, vec2_subtract(before, after));
        }

        if (graphic_is_key_pressed(GRAPHIC_KEY_SPACE)) {
            blend_mode = (blend_mode + 1) % GRAPHIC_BLEND_MODE_COUNT;
            printf("blend mode %d\n", blend_mode);
        }

        graphic_drawing_begin();

        graphic_color_t background = { 64, 64, 64, 255 };
        graphic_background_clear(background);

        /* left half: through the camera, clipped to its side */
        graphic_mode_scissor_begin(0, 0, half_width, graphic_render_height_get());
        graphic_mode2d_begin(camera);
        world_draw();
        graphic_mode2d_end();
        graphic_mode_scissor_end();

        /* right half: screen space, one row per blend mode */
        for (int i = 0; i < GRAPHIC_BLEND_MODE_COUNT; i++)
            blend_sample_draw((float)half_width + 40.0f, 40.0f + (float)i * 80.0f, i);

        /* the mode selected with space, on its own on the far right */
        blend_sample_draw((float)half_width + 260.0f, 180.0f, blend_mode);

        graphic_drawing_end();
    }

    graphic_window_close();

    return 0;
}