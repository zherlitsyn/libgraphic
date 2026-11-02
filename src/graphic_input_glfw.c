/*
 * No GLFW_INCLUDE_VULKAN here, unlike the window backend beside it:
 * nothing in this file touches Vulkan, and asking GLFW to pull the
 * Vulkan headers in made a file that needs neither depend on both.
 */
#include <GLFW/glfw3.h>

#include <math.h>
#include <string.h>

#include "graphic_platform.h"

/*
 * Desktop input. Everything works on a two frame history: the current
 * state is what the window system told us, the previous state is what
 * it told us last frame, and the four predicates are comparisons
 * between the two.
 *
 *   down     = current
 *   up       = !current
 *   pressed  = current && !previous
 *   released = !current && previous
 *
 * That is why graphic_events_poll() must run exactly once per frame:
 * calling it twice makes every press invisible to the frame in
 * between.
 *
 * A two frame history cannot express order, nor an event that began
 * and ended inside one frame.
 */

#define MAX_KEYBOARD_KEYS         512
#define MAX_MOUSE_BUTTONS         8
#define MAX_KEY_PRESSED_QUEUE     16
#define MAX_CHAR_PRESSED_QUEUE    16

struct keyboard_state {
    char current[MAX_KEYBOARD_KEYS];
    char previous[MAX_KEYBOARD_KEYS];
    char repeat[MAX_KEYBOARD_KEYS];    /* auto repeat, this frame */

    int  key_queue[MAX_KEY_PRESSED_QUEUE];
    int  key_queue_count;
    int  char_queue[MAX_CHAR_PRESSED_QUEUE];
    int  char_queue_count;

    int  exit_key;
};

struct mouse_state {
    char        current[MAX_MOUSE_BUTTONS];
    char        previous[MAX_MOUSE_BUTTONS];

    vec2_t      position;
    vec2_t      position_previous;
    vec2_t      offset;
    vec2_t      scale;

    vec2_t      wheel;                 /* accumulated by the last poll */

    int         cursor_shape;
    GLFWcursor *cursors[16];
};

static struct {
    struct keyboard_state keyboard;
    struct mouse_state mouse;
} input;

/* ------------------------------------------------------------------ */
/* callbacks                                                          */
/* ------------------------------------------------------------------ */

static void key_callback(GLFWwindow *handle,
                         int         key,
                         int         scancode,
                         int         action,
                         int         mods)
{
    (void)scancode;
    (void)mods;

    if (key < 0 || key >= MAX_KEYBOARD_KEYS)
        return;

    if (action == GLFW_RELEASE) {
        input.keyboard.current[key] = 0;
    } else if (action == GLFW_PRESS) {
        input.keyboard.current[key] = 1;

        if (key == input.keyboard.exit_key)
            glfwSetWindowShouldClose(handle, GLFW_TRUE);

        if (input.keyboard.key_queue_count < MAX_KEY_PRESSED_QUEUE)
            input.keyboard.key_queue[input.keyboard.key_queue_count++] = key;

    } else if (action == GLFW_REPEAT) {
        /*
         * Not a fresh press, so it does not join the queue the
         * per frame predicates read; graphic_is_key_pressed_repeat()
         * reports it instead.
         */
        input.keyboard.repeat[key] = 1;
    }
}

static void char_callback(GLFWwindow  *handle,
                          unsigned int codepoint)
{
    (void)handle;

    if (input.keyboard.char_queue_count < MAX_CHAR_PRESSED_QUEUE)
        input.keyboard.char_queue[input.keyboard.char_queue_count++] = (int)codepoint;
}

static void mouse_button_callback(GLFWwindow *handle,
                                  int         button,
                                  int         action,
                                  int         mods)
{
    (void)handle;
    (void)mods;

    if (button < 0 || button >= MAX_MOUSE_BUTTONS)
        return;

    input.mouse.current[button] = (action == GLFW_PRESS) ? 1 : 0;
}

static void cursor_position_callback(GLFWwindow *handle,
                                     double      x,
                                     double      y)
{
    (void)handle;

    input.mouse.position.x = (float)x;
    input.mouse.position.y = (float)y;
}

static void scroll_callback(GLFWwindow *handle,
                            double      x,
                            double      y)
{
    (void)handle;

    input.mouse.wheel.x += (float)x;
    input.mouse.wheel.y += (float)y;
}

/* ------------------------------------------------------------------ */
/* internal hooks                                                     */
/* ------------------------------------------------------------------ */

void graphic_platform_input_init(void)
{
    GLFWwindow *handle = graphic_window_handle_get();

    memset(&input, 0, sizeof(input));

    input.keyboard.exit_key = GRAPHIC_KEY_ESCAPE;
    input.mouse.scale.x = 1.0f;
    input.mouse.scale.y = 1.0f;

    if (handle == NULL)
        return;

    glfwSetKeyCallback(handle, key_callback);
    glfwSetCharCallback(handle, char_callback);
    glfwSetMouseButtonCallback(handle, mouse_button_callback);
    glfwSetCursorPosCallback(handle, cursor_position_callback);
    glfwSetScrollCallback(handle, scroll_callback);
}

void graphic_platform_input_shutdown(void)
{
    for (size_t i = 0; i < sizeof(input.mouse.cursors) /
                           sizeof(input.mouse.cursors[0]); i++) {
        if (input.mouse.cursors[i] != NULL) {
            glfwDestroyCursor(input.mouse.cursors[i]);
            input.mouse.cursors[i] = NULL;
        }
    }
}

/*
 * Runs immediately before the window system is pumped: the state that
 * was current becomes previous, and everything that only lives for one
 * frame is cleared.
 */
void graphic_platform_input_prepare(void)
{
    memcpy(input.keyboard.previous, input.keyboard.current, MAX_KEYBOARD_KEYS);
    memset(input.keyboard.repeat, 0, MAX_KEYBOARD_KEYS);
    input.keyboard.key_queue_count = 0;
    input.keyboard.char_queue_count = 0;

    memcpy(input.mouse.previous, input.mouse.current, MAX_MOUSE_BUTTONS);
    input.mouse.position_previous = input.mouse.position;
    input.mouse.wheel.x = 0.0f;
    input.mouse.wheel.y = 0.0f;
}

/* ------------------------------------------------------------------ */
/* keyboard                                                           */
/* ------------------------------------------------------------------ */

static bool key_valid(int key)
{
    return key > 0 && key < MAX_KEYBOARD_KEYS;
}

bool graphic_is_key_pressed(int key)
{
    if (!key_valid(key))
        return false;

    return input.keyboard.current[key] && !input.keyboard.previous[key];
}

bool graphic_is_key_pressed_repeat(int key)
{
    if (!key_valid(key))
        return false;

    return input.keyboard.repeat[key] != 0;
}

bool graphic_is_key_down(int key)
{
    if (!key_valid(key))
        return false;

    return input.keyboard.current[key] != 0;
}

bool graphic_is_key_released(int key)
{
    if (!key_valid(key))
        return false;

    return !input.keyboard.current[key] && input.keyboard.previous[key];
}

bool graphic_is_key_up(int key)
{
    if (!key_valid(key))
        return false;

    return input.keyboard.current[key] == 0;
}

/*
 * Both queues are drained from the front, so a text field can loop
 * until the call returns zero.
 */
int graphic_key_pressed_get(void)
{
    if (input.keyboard.key_queue_count == 0)
        return 0;

    int key = input.keyboard.key_queue[0];

    for (int i = 0; i < input.keyboard.key_queue_count - 1; i++)
        input.keyboard.key_queue[i] = input.keyboard.key_queue[i + 1];

    input.keyboard.key_queue_count--;

    return key;
}

int graphic_char_pressed_get(void)
{
    if (input.keyboard.char_queue_count == 0)
        return 0;

    int codepoint = input.keyboard.char_queue[0];

    for (int i = 0; i < input.keyboard.char_queue_count - 1; i++)
        input.keyboard.char_queue[i] = input.keyboard.char_queue[i + 1];

    input.keyboard.char_queue_count--;

    return codepoint;
}

/* layout dependent: the same key is "q" or "й" depending on the layout */
const char *graphic_key_name_get(int key)
{
    const char *name = glfwGetKeyName(key, 0);
    return (name != NULL) ? name : "";
}

void graphic_exit_key_set(int key)
{
    input.keyboard.exit_key = key;
}

/* ------------------------------------------------------------------ */
/* mouse                                                              */
/* ------------------------------------------------------------------ */

static bool mouse_button_valid(int button)
{
    return button >= 0 && button < MAX_MOUSE_BUTTONS;
}

bool graphic_is_mouse_button_pressed(int button)
{
    if (!mouse_button_valid(button))
        return false;

    return input.mouse.current[button] && !input.mouse.previous[button];
}

bool graphic_is_mouse_button_down(int button)
{
    if (!mouse_button_valid(button))
        return false;

    return input.mouse.current[button] != 0;
}

bool graphic_is_mouse_button_released(int button)
{
    if (!mouse_button_valid(button))
        return false;

    return !input.mouse.current[button] && input.mouse.previous[button];
}

bool graphic_is_mouse_button_up(int button)
{
    if (!mouse_button_valid(button))
        return false;

    return input.mouse.current[button] == 0;
}

/*
 * Offset and scale let a caller map raw window coordinates onto a
 * virtual resolution, which is what a letterboxed render target needs.
 */
vec2_t graphic_mouse_position_get(void)
{
    vec2_t position;
    position.x = (input.mouse.position.x + input.mouse.offset.x) * input.mouse.scale.x;
    position.y = (input.mouse.position.y + input.mouse.offset.y) * input.mouse.scale.y;

    return position;
}

int graphic_mouse_x_get(void)
{
    return (int)graphic_mouse_position_get().x;
}

int graphic_mouse_y_get(void)
{
    return (int)graphic_mouse_position_get().y;
}

vec2_t graphic_mouse_delta_get(void)
{
    vec2_t delta;
    delta.x = input.mouse.position.x - input.mouse.position_previous.x;
    delta.y = input.mouse.position.y - input.mouse.position_previous.y;

    return delta;
}

void graphic_mouse_position_set(int x,
                                int y)
{
    GLFWwindow *handle = graphic_window_handle_get();

    input.mouse.position.x = (float)x;
    input.mouse.position.y = (float)y;
    input.mouse.position_previous = input.mouse.position;

    if (handle != NULL)
        glfwSetCursorPos(handle, (double)x, (double)y);
}

void graphic_mouse_offset_set(int offset_x,
                              int offset_y)
{
    input.mouse.offset.x = (float)offset_x;
    input.mouse.offset.y = (float)offset_y;
}

void graphic_mouse_scale_set(float scale_x,
                             float scale_y)
{
    input.mouse.scale.x = scale_x;
    input.mouse.scale.y = scale_y;
}

float graphic_mouse_wheel_move_get(void)
{
    vec2_t wheel = input.mouse.wheel;

    /*
     * A plain wheel only moves in Y; a touchpad moves in both, so
     * the axis that moved further wins. Compared by magnitude: the
     * first version of this compared the values themselves, and
     * scrolling down made the test x > y true at x = 0, so it
     * returned zero and the wheel only worked one way.
     */
    if (fabsf(wheel.x) > fabsf(wheel.y))
        return wheel.x;

    return wheel.y;
}

vec2_t graphic_mouse_wheel_move_v_get(void)
{
    return input.mouse.wheel;
}

void graphic_mouse_cursor_set(int cursor)
{
    /*
     * The six shapes below arrived in GLFW 3.4, and the project asks
     * for 3.3. On 3.3 they fall back to the nearest older shape
     * rather than refusing to build: a pointing hand shown as an
     * arrow is a cosmetic loss, a failed build is not.
     */
#ifndef GLFW_POINTING_HAND_CURSOR
#define GLFW_POINTING_HAND_CURSOR    GLFW_HAND_CURSOR
#endif
#ifndef GLFW_RESIZE_EW_CURSOR
#define GLFW_RESIZE_EW_CURSOR        GLFW_HRESIZE_CURSOR
#endif
#ifndef GLFW_RESIZE_NS_CURSOR
#define GLFW_RESIZE_NS_CURSOR        GLFW_VRESIZE_CURSOR
#endif
#ifndef GLFW_RESIZE_NWSE_CURSOR
#define GLFW_RESIZE_NWSE_CURSOR      GLFW_ARROW_CURSOR
#endif
#ifndef GLFW_RESIZE_NESW_CURSOR
#define GLFW_RESIZE_NESW_CURSOR      GLFW_ARROW_CURSOR
#endif
#ifndef GLFW_RESIZE_ALL_CURSOR
#define GLFW_RESIZE_ALL_CURSOR       GLFW_ARROW_CURSOR
#endif
#ifndef GLFW_NOT_ALLOWED_CURSOR
#define GLFW_NOT_ALLOWED_CURSOR      GLFW_ARROW_CURSOR
#endif

    static const int shape_map[] = {
        [GRAPHIC_MOUSE_CURSOR_DEFAULT]       = GLFW_ARROW_CURSOR,
        [GRAPHIC_MOUSE_CURSOR_ARROW]         = GLFW_ARROW_CURSOR,
        [GRAPHIC_MOUSE_CURSOR_IBEAM]         = GLFW_IBEAM_CURSOR,
        [GRAPHIC_MOUSE_CURSOR_CROSSHAIR]     = GLFW_CROSSHAIR_CURSOR,
        [GRAPHIC_MOUSE_CURSOR_POINTING_HAND] = GLFW_POINTING_HAND_CURSOR,
        [GRAPHIC_MOUSE_CURSOR_RESIZE_EW]     = GLFW_RESIZE_EW_CURSOR,
        [GRAPHIC_MOUSE_CURSOR_RESIZE_NS]     = GLFW_RESIZE_NS_CURSOR,
        [GRAPHIC_MOUSE_CURSOR_RESIZE_NWSE]   = GLFW_RESIZE_NWSE_CURSOR,
        [GRAPHIC_MOUSE_CURSOR_RESIZE_NESW]   = GLFW_RESIZE_NESW_CURSOR,
        [GRAPHIC_MOUSE_CURSOR_RESIZE_ALL]    = GLFW_RESIZE_ALL_CURSOR,
        [GRAPHIC_MOUSE_CURSOR_NOT_ALLOWED]   = GLFW_NOT_ALLOWED_CURSOR,
    };

    GLFWwindow *handle = graphic_window_handle_get();

    if (handle == NULL || cursor < 0 || cursor > GRAPHIC_MOUSE_CURSOR_NOT_ALLOWED)
        return;

    /* created once and cached: they are window system objects */
    if (input.mouse.cursors[cursor] == NULL)
        input.mouse.cursors[cursor] = glfwCreateStandardCursor(shape_map[cursor]);

    glfwSetCursor(handle, input.mouse.cursors[cursor]);
    input.mouse.cursor_shape = cursor;
}