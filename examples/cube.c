#include "graphic.h"

static void cube_draw(vec3_t          position,
                      float           width,
                      float           height,
                      float           length,
                      graphic_color_t color)
{
    float x = width  / 2.0f;
    float y = height / 2.0f;
    float z = length / 2.0f;

    graphic_matrix_push();
    graphic_translatef(position.x, position.y, position.z);

    graphic_begin(GRAPHIC_DRAW_QUADS);
    graphic_color4ub(color.r, color.g, color.b, color.a);

    /* front */
    graphic_normal3f(0.0f, 0.0f, 1.0f);
    graphic_vertex3f(-x, -y,  z);
    graphic_vertex3f( x, -y,  z);
    graphic_vertex3f( x,  y,  z);
    graphic_vertex3f(-x,  y,  z);

    /* back */
    graphic_normal3f(0.0f, 0.0f, -1.0f);
    graphic_vertex3f( x, -y, -z);
    graphic_vertex3f(-x, -y, -z);
    graphic_vertex3f(-x,  y, -z);
    graphic_vertex3f( x,  y, -z);

    /* top */
    graphic_normal3f(0.0f, 1.0f, 0.0f);
    graphic_vertex3f(-x,  y,  z);
    graphic_vertex3f( x,  y,  z);
    graphic_vertex3f( x,  y, -z);
    graphic_vertex3f(-x,  y, -z);

    /* bottom */
    graphic_normal3f(0.0f, -1.0f, 0.0f);
    graphic_vertex3f(-x, -y, -z);
    graphic_vertex3f( x, -y, -z);
    graphic_vertex3f( x, -y,  z);
    graphic_vertex3f(-x, -y,  z);

    /* right */
    graphic_normal3f(1.0f, 0.0f, 0.0f);
    graphic_vertex3f( x, -y,  z);
    graphic_vertex3f( x, -y, -z);
    graphic_vertex3f( x,  y, -z);
    graphic_vertex3f( x,  y,  z);

    /* left */
    graphic_normal3f(-1.0f, 0.0f, 0.0f);
    graphic_vertex3f(-x, -y, -z);
    graphic_vertex3f(-x, -y,  z);
    graphic_vertex3f(-x,  y,  z);
    graphic_vertex3f(-x,  y, -z);

    graphic_end();

    graphic_matrix_pop();
}

static void grid_draw(int slices, float spacing)
{
    float half = (float)slices * spacing / 2.0f;

    graphic_begin(GRAPHIC_DRAW_LINES);
    graphic_color4ub(128, 128, 128, 255);

    for (int i = 0; i <= slices; i++) {
        float offset = -half + (float)i * spacing;

        graphic_vertex3f(offset, 0.0f, -half);
        graphic_vertex3f(offset, 0.0f,  half);
        graphic_vertex3f(-half,  0.0f, offset);
        graphic_vertex3f( half,  0.0f, offset);
    }

    graphic_end();
}

int main(void)
{
    graphic_camera3d_t camera;
    camera.position   = (vec3_t){ 4.0f, 4.0f, 4.0f };
    camera.target     = (vec3_t){ 0.0f, 0.0f, 0.0f };
    camera.up         = (vec3_t){ 0.0f, 1.0f, 0.0f };
    camera.fovy       = 45.0f;
    camera.projection = GRAPHIC_CAMERA_PERSPECTIVE;

    graphic_window_flags_set(GRAPHIC_WINDOW_RESIZABLE |
                             GRAPHIC_WINDOW_VSYNC     |
                             GRAPHIC_WINDOW_VALIDATION);

    if (!graphic_window(960, 480, "graphic - basic cube"))
        return 1;

    float angle = 0.0f;
    while (!graphic_window_should_close()) {
        angle += 45.0f * graphic_frame_time_get();

        graphic_drawing_begin();

        graphic_color_t background = { 64, 64, 64, 255 };
        graphic_background_clear(background);

        graphic_mode3d_begin(camera);

        grid_draw(20, 0.5f);

        vec3_t          cube_position = { 0.0f, 1.0f, 0.0f };
        graphic_color_t cube_color    = { 255, 0, 0, 255 };

        graphic_matrix_push();
        graphic_rotatef(angle, 0.0f, 1.0f, 0.0f);
        cube_draw(cube_position, 2.0f, 2.0f, 2.0f, cube_color);
        graphic_matrix_pop();

        graphic_mode3d_end();

        graphic_drawing_end();
    }

    graphic_window_close();

    return 0;
}