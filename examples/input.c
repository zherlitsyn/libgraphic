#include <stdio.h>

#include "graphic.h"

/*
 * Exercises the input layer. Everything is reported on change rather
 * than every frame, so the output stays readable without any drawing
 * of text, which the library cannot do yet.
 */
static void keyboard_report(void)
{
    int key;
    int codepoint;

    while ((key = graphic_key_pressed_get()) != 0)
        printf("key pressed: %d (%s)\n", key, graphic_key_name_get(key));

    while ((codepoint = graphic_char_pressed_get()) != 0)
        printf("char typed: U+%04X\n", codepoint);

    if (graphic_is_key_pressed(GRAPHIC_KEY_SPACE))
        printf("space pressed\n");

    if (graphic_is_key_released(GRAPHIC_KEY_SPACE))
        printf("space released\n");

    if (graphic_is_key_pressed_repeat(GRAPHIC_KEY_BACKSPACE))
        printf("backspace repeating\n");

    if (graphic_is_key_down(GRAPHIC_KEY_LEFT_SHIFT) &&
        graphic_is_key_pressed(GRAPHIC_KEY_F))
        graphic_window_fullscreen_toggle();
}

static void mouse_report(void)
{
    static int cursor_shape = GRAPHIC_MOUSE_CURSOR_DEFAULT;

    for (int button = GRAPHIC_MOUSE_BUTTON_LEFT; 
             button <= GRAPHIC_MOUSE_BUTTON_MIDDLE; button++) {
        if (graphic_is_mouse_button_pressed(button)) {
            vec2_t position = graphic_mouse_position_get();
            printf("mouse button %d pressed at %.0f,%.0f\n",
                   button, position.x, position.y);
        }
        if (graphic_is_mouse_button_released(button))
            printf("mouse button %d released\n", button);
    }

    vec2_t delta = graphic_mouse_delta_get();
    if (delta.x != 0.0f || delta.y != 0.0f) {
        if (graphic_is_mouse_button_down(GRAPHIC_MOUSE_BUTTON_RIGHT))
            printf("drag delta %.1f,%.1f\n", delta.x, delta.y);
    }

    float wheel = graphic_mouse_wheel_move_get();
    if (wheel != 0.0f) {
        printf("wheel %.1f\n", wheel);

        /* cycle the cursor shape with the wheel */
        cursor_shape += (wheel > 0.0f) ? 1 : -1;
        if (cursor_shape < 0)
            cursor_shape = GRAPHIC_MOUSE_CURSOR_NOT_ALLOWED;
        if (cursor_shape > GRAPHIC_MOUSE_CURSOR_NOT_ALLOWED)
            cursor_shape = 0;

        graphic_mouse_cursor_set(cursor_shape);
    }
}

static void gamepad_report(void)
{
    static bool was_available[4];

    for (int pad = 0; pad < 4; pad++) {
        bool available = graphic_is_gamepad_available(pad);

        if (available != was_available[pad]) {
            printf("gamepad %d %s: %s\n", pad,
                   available ? "connected" : "disconnected",
                   graphic_gamepad_name_get(pad));
            was_available[pad] = available;
        }

        if (!available)
            continue;

        for (int button = GRAPHIC_GAMEPAD_BUTTON_LEFT_FACE_UP;
                 button <= GRAPHIC_GAMEPAD_BUTTON_RIGHT_THUMB; button++) {
            if (graphic_is_gamepad_button_pressed(pad, button))
                printf("gamepad %d button %d pressed\n", pad, button);
        }

        if (graphic_is_gamepad_button_down(pad,
                GRAPHIC_GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
            printf("pad %d sticks: %.2f,%.2f  %.2f,%.2f\n", pad,
                   graphic_gamepad_axis_movement_get(pad, GRAPHIC_GAMEPAD_AXIS_LEFT_X),
                   graphic_gamepad_axis_movement_get(pad, GRAPHIC_GAMEPAD_AXIS_LEFT_Y),
                   graphic_gamepad_axis_movement_get(pad, GRAPHIC_GAMEPAD_AXIS_RIGHT_X),
                   graphic_gamepad_axis_movement_get(pad, GRAPHIC_GAMEPAD_AXIS_RIGHT_Y));
        }
    }
}

static void touch_report(void)
{
    int count = graphic_touch_point_count_get();

    if (count > 0) {
        vec2_t position = graphic_touch_position_get(0);

        printf("touch %d point(s), first id %d at %.0f,%.0f\n",
               count,
               graphic_touch_point_id_get(0),
               position.x, position.y);
    }
}

int main(void)
{
    graphic_camera3d_t camera;
    camera.position = (vec3_t){ 4.0f, 4.0f, 4.0f };
    camera.target = (vec3_t){ 0.0f, 0.0f, 0.0f };
    camera.up = (vec3_t){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = GRAPHIC_CAMERA_PERSPECTIVE;

    graphic_window_flags_set(GRAPHIC_WINDOW_RESIZABLE |
                             GRAPHIC_WINDOW_VSYNC     |
                             GRAPHIC_WINDOW_VALIDATION);

    if (!graphic_window(960, 540, "graphic - input"))
        return 1;

    graphic_fps_target_set(60);
    graphic_exit_key_set(GRAPHIC_KEY_ESCAPE);

    printf("esc quits, shift+f toggles fullscreen, wheel cycles the "
           "cursor, hold right button and drag\n");

    float angle = 0.0f;
    while (!graphic_window_should_close()) {
        keyboard_report();
        mouse_report();
        gamepad_report();
        touch_report();

        /* arrow keys spin the grid, to prove the state persists */
        if (graphic_is_key_down(GRAPHIC_KEY_LEFT))
            angle -= 90.0f * graphic_frame_time_get();

        if (graphic_is_key_down(GRAPHIC_KEY_RIGHT))
            angle += 90.0f * graphic_frame_time_get();

        graphic_drawing_begin();

        graphic_color_t background = { 64, 64, 64, 255 };
        graphic_background_clear(background);

        graphic_mode3d_begin(camera);
        graphic_matrix_push();
        graphic_rotatef(angle, 0.0f, 1.0f, 0.0f);

        graphic_begin(GRAPHIC_DRAW_LINES);
        graphic_color4ub(128, 128, 128, 255);
        graphic_vertex3f(-2.0f, 0.0f, 0.0f);
        graphic_vertex3f(2.0f, 0.0f, 0.0f);
        graphic_vertex3f(0.0f, 0.0f, -2.0f);
        graphic_vertex3f(0.0f, 0.0f, 2.0f);
        graphic_vertex3f(0.0f, -2.0f, 0.0f);
        graphic_vertex3f(0.0f, 2.0f, 0.0f);
        graphic_end();

        graphic_matrix_pop();
        graphic_mode3d_end();

        graphic_drawing_end();
    }

    graphic_window_close();

    return 0;
}