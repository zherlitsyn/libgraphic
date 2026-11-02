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
 * A two frame history cannot express order, and it cannot express an
 * event that began and ended inside one frame. Callers that need
 * either register callbacks: every GLFW callback below updates the
 * state first and then hands the event over untouched.
 */

#define MAX_KEYBOARD_KEYS            512
#define MAX_MOUSE_BUTTONS            8
#define MAX_GAMEPADS                 4
#define MAX_GAMEPAD_BUTTONS          32
#define MAX_GAMEPAD_AXES             8
#define MAX_TOUCH_POINTS             8
#define MAX_KEY_PRESSED_QUEUE        16
#define MAX_CHAR_PRESSED_QUEUE       16
#define MAX_GAMEPAD_NAME             64

#define GAMEPAD_TRIGGER_THRESHOLD    0.1f

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

struct gamepad_state {
    bool  ready[MAX_GAMEPADS];
    char  name[MAX_GAMEPADS][MAX_GAMEPAD_NAME];
    char  current[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS];
    char  previous[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS];
    float axis[MAX_GAMEPADS][MAX_GAMEPAD_AXES];
    int   axis_count[MAX_GAMEPADS];
    int   last_button_pressed;
};

struct touch_state {
    vec2_t position[MAX_TOUCH_POINTS];
    int    id[MAX_TOUCH_POINTS];
    int    count;
};

/*
 * One slot per event, each with its own context, so registering an
 * interest in the scroll wheel does not mean naming the other four.
 */
struct callback_state {
    graphic_mouse_move_callback_t   mouse_move;
    void                           *mouse_move_context;

    graphic_mouse_button_callback_t mouse_button;
    void                           *mouse_button_context;

    graphic_scroll_callback_t       scroll;
    void                           *scroll_context;

    graphic_key_callback_t          key;
    void                           *key_context;

    graphic_text_callback_t         text;
    void                           *text_context;
};

static struct {
    struct keyboard_state keyboard;
    struct mouse_state    mouse;
    struct gamepad_state  gamepad;
    struct touch_state    touch;
    struct callback_state callback;
} input;

void graphic_mouse_move_callback_set(graphic_mouse_move_callback_t callback,
                                     void                         *context)
{
    input.callback.mouse_move         = callback;
    input.callback.mouse_move_context = context;
}

void graphic_mouse_button_callback_set(graphic_mouse_button_callback_t callback,
                                       void                           *context)
{
    input.callback.mouse_button         = callback;
    input.callback.mouse_button_context = context;
}

void graphic_scroll_callback_set(graphic_scroll_callback_t callback,
                                 void                     *context)
{
    input.callback.scroll         = callback;
    input.callback.scroll_context = context;
}

void graphic_key_callback_set(graphic_key_callback_t callback,
                              void                  *context)
{
    input.callback.key         = callback;
    input.callback.key_context = context;
}

void graphic_text_callback_set(graphic_text_callback_t callback,
                               void                   *context)
{
    input.callback.text         = callback;
    input.callback.text_context = context;
}

void graphic_input_callbacks_clear(void)
{
    memset(&input.callback, 0, sizeof(input.callback));
}

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
         * per frame predicates read. It is still an event, and
         * a caller driving a text field wants it, so it falls
         * through to the callback rather than returning here.
         */
        input.keyboard.repeat[key] = 1;
    }

    /*
     * A repeat counts as down, because the key is: the flag says
     * whether the window system generated this one from a held key
     * rather than from a fresh press.
     */
    if (input.callback.key != NULL)
        input.callback.key(input.callback.key_context,
                           key,
                           action != GLFW_RELEASE,
                           action == GLFW_REPEAT);
}

static void char_callback(GLFWwindow  *handle,
                          unsigned int codepoint)
{
    (void)handle;

    if (input.keyboard.char_queue_count < MAX_CHAR_PRESSED_QUEUE)
        input.keyboard.char_queue[input.keyboard.char_queue_count++] = (int)codepoint;

    if (input.callback.text != NULL)
        input.callback.text(input.callback.text_context, (int)codepoint);
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

    /* the desktop stand in for a touch point */
    if (button == GRAPHIC_MOUSE_BUTTON_LEFT) {
        input.touch.count       = (action == GLFW_PRESS) ? 1 : 0;
        input.touch.position[0] = input.mouse.position;
        input.touch.id[0]       = 0;
    }

    /*
     * GLFW does not put the cursor in this callback, but a click
     * carries the place it happened, so it is passed along.
     */
    if (input.callback.mouse_button != NULL)
        input.callback.mouse_button(input.callback.mouse_button_context,
                                    button,
                                    action == GLFW_PRESS,
                                    input.mouse.position.x,
                                    input.mouse.position.y);
}

static void cursor_position_callback(GLFWwindow *handle,
                                     double      x,
                                     double      y)
{
    (void)handle;

    input.mouse.position.x  = (float)x;
    input.mouse.position.y  = (float)y;
    input.touch.position[0] = input.mouse.position;

    if (input.callback.mouse_move != NULL)
        input.callback.mouse_move(input.callback.mouse_move_context,
                                  input.mouse.position.x,
                                  input.mouse.position.y);
}

static void scroll_callback(GLFWwindow *handle,
                            double      x,
                            double      y)
{
    (void)handle;

    input.mouse.wheel.x += (float)x;
    input.mouse.wheel.y += (float)y;

    if (input.callback.scroll != NULL)
        input.callback.scroll(input.callback.scroll_context, (float)x, (float)y);
}

static void joystick_callback(int joystick, int event)
{
    if (joystick < 0 || joystick >= MAX_GAMEPADS)
        return;

    if (event == GLFW_DISCONNECTED) {
        input.gamepad.ready[joystick] = false;
        memset(input.gamepad.name[joystick],    0, MAX_GAMEPAD_NAME);
        memset(input.gamepad.current[joystick], 0, MAX_GAMEPAD_BUTTONS);
    }
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
    glfwSetJoystickCallback(joystick_callback);
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

    memcpy(input.gamepad.previous, input.gamepad.current, sizeof(input.gamepad.current));
    input.gamepad.last_button_pressed = GRAPHIC_GAMEPAD_BUTTON_UNKNOWN;
}

/*
 * Gamepads are the one device GLFW does not report through callbacks,
 * so they are polled after the event pump.
 */
void graphic_platform_input_gamepads_update(void)
{
    /* libgraphic button order -> GLFW gamepad button order */
    static const int button_map[] = {
        [GRAPHIC_GAMEPAD_BUTTON_LEFT_FACE_UP]     = GLFW_GAMEPAD_BUTTON_DPAD_UP,
        [GRAPHIC_GAMEPAD_BUTTON_LEFT_FACE_RIGHT]  = GLFW_GAMEPAD_BUTTON_DPAD_RIGHT,
        [GRAPHIC_GAMEPAD_BUTTON_LEFT_FACE_DOWN]   = GLFW_GAMEPAD_BUTTON_DPAD_DOWN,
        [GRAPHIC_GAMEPAD_BUTTON_LEFT_FACE_LEFT]   = GLFW_GAMEPAD_BUTTON_DPAD_LEFT,
        [GRAPHIC_GAMEPAD_BUTTON_RIGHT_FACE_UP]    = GLFW_GAMEPAD_BUTTON_TRIANGLE,
        [GRAPHIC_GAMEPAD_BUTTON_RIGHT_FACE_RIGHT] = GLFW_GAMEPAD_BUTTON_CIRCLE,
        [GRAPHIC_GAMEPAD_BUTTON_RIGHT_FACE_DOWN]  = GLFW_GAMEPAD_BUTTON_CROSS,
        [GRAPHIC_GAMEPAD_BUTTON_RIGHT_FACE_LEFT]  = GLFW_GAMEPAD_BUTTON_SQUARE,
        [GRAPHIC_GAMEPAD_BUTTON_LEFT_TRIGGER_1]   = GLFW_GAMEPAD_BUTTON_LEFT_BUMPER,
        [GRAPHIC_GAMEPAD_BUTTON_RIGHT_TRIGGER_1]  = GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER,
        [GRAPHIC_GAMEPAD_BUTTON_MIDDLE_LEFT]      = GLFW_GAMEPAD_BUTTON_BACK,
        [GRAPHIC_GAMEPAD_BUTTON_MIDDLE]           = GLFW_GAMEPAD_BUTTON_GUIDE,
        [GRAPHIC_GAMEPAD_BUTTON_MIDDLE_RIGHT]     = GLFW_GAMEPAD_BUTTON_START,
        [GRAPHIC_GAMEPAD_BUTTON_LEFT_THUMB]       = GLFW_GAMEPAD_BUTTON_LEFT_THUMB,
        [GRAPHIC_GAMEPAD_BUTTON_RIGHT_THUMB]      = GLFW_GAMEPAD_BUTTON_RIGHT_THUMB,
    };

    for (int pad = 0; pad < MAX_GAMEPADS; pad++) {
        if (!glfwJoystickPresent(pad)) {
            input.gamepad.ready[pad] = false;
            continue;
        }

        input.gamepad.ready[pad] = true;

        const char *name = glfwGetGamepadName(pad);
        if (name == NULL)
            name = glfwGetJoystickName(pad);
        if (name != NULL) {
            strncpy(input.gamepad.name[pad], name, MAX_GAMEPAD_NAME - 1);
            input.gamepad.name[pad][MAX_GAMEPAD_NAME - 1] = '\0';
        }

        GLFWgamepadstate state;
        if (!glfwGetGamepadState(pad, &state)) {
            /*
             * No SDL mapping for this device. Axes and
             * buttons exist but their meaning is unknown,
             * so only the raw axis count is reported.
             */
            glfwGetJoystickAxes(pad, &input.gamepad.axis_count[pad]);
            continue;
        }

        input.gamepad.axis_count[pad] = 6;

        input.gamepad.axis[pad][GRAPHIC_GAMEPAD_AXIS_LEFT_X]        = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
        input.gamepad.axis[pad][GRAPHIC_GAMEPAD_AXIS_LEFT_Y]        = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
        input.gamepad.axis[pad][GRAPHIC_GAMEPAD_AXIS_RIGHT_X]       = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
        input.gamepad.axis[pad][GRAPHIC_GAMEPAD_AXIS_RIGHT_Y]       = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
        input.gamepad.axis[pad][GRAPHIC_GAMEPAD_AXIS_LEFT_TRIGGER]  = state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
        input.gamepad.axis[pad][GRAPHIC_GAMEPAD_AXIS_RIGHT_TRIGGER] = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];

        for (int button = GRAPHIC_GAMEPAD_BUTTON_LEFT_FACE_UP;
                 button <= GRAPHIC_GAMEPAD_BUTTON_RIGHT_THUMB; button++) {

            int pressed;

            /* the analogue triggers have no digital button */
            if (button == GRAPHIC_GAMEPAD_BUTTON_LEFT_TRIGGER_2) {
                pressed = input.gamepad.axis[pad][GRAPHIC_GAMEPAD_AXIS_LEFT_TRIGGER] > GAMEPAD_TRIGGER_THRESHOLD;
            } else if (button == GRAPHIC_GAMEPAD_BUTTON_RIGHT_TRIGGER_2) {
                pressed = input.gamepad.axis[pad][GRAPHIC_GAMEPAD_AXIS_RIGHT_TRIGGER] > GAMEPAD_TRIGGER_THRESHOLD;
            } else {
                pressed = state.buttons[button_map[button]] == GLFW_PRESS;
            }

            input.gamepad.current[pad][button] = pressed ? 1 : 0;

            if (pressed && !input.gamepad.previous[pad][button])
                input.gamepad.last_button_pressed = button;
        }
    }
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
/* gamepad                                                             */
/* ------------------------------------------------------------------ */

static bool gamepad_valid(int gamepad,
                          int button)
{
    return gamepad >= 0                  &&
           gamepad < MAX_GAMEPADS        &&
           button  >= 0                  &&
           button  < MAX_GAMEPAD_BUTTONS &&
           input.gamepad.ready[gamepad];
}

bool graphic_is_gamepad_available(int gamepad)
{
    if (gamepad < 0 || gamepad >= MAX_GAMEPADS)
        return false;

    return input.gamepad.ready[gamepad];
}

const char *graphic_gamepad_name_get(int gamepad)
{
    if (gamepad < 0 || gamepad >= MAX_GAMEPADS)
        return "";

    return input.gamepad.name[gamepad];
}

bool graphic_is_gamepad_button_pressed(int gamepad,
                                       int button)
{
    if (!gamepad_valid(gamepad, button))
        return false;

    return input.gamepad.current[gamepad][button] &&
         !input.gamepad.previous[gamepad][button];
}

bool graphic_is_gamepad_button_down(int gamepad,
                                    int button)
{
    if (!gamepad_valid(gamepad, button))
        return false;

    return input.gamepad.current[gamepad][button] != 0;
}

bool graphic_is_gamepad_button_released(int gamepad,
                                        int button)
{
    if (!gamepad_valid(gamepad, button))
        return false;

    return !input.gamepad.current[gamepad][button] &&
           input.gamepad.previous[gamepad][button];
}

bool graphic_is_gamepad_button_up(int gamepad,
                                  int button)
{
    if (!gamepad_valid(gamepad, button))
        return false;

    return input.gamepad.current[gamepad][button] == 0;
}

int graphic_gamepad_button_pressed_get(void)
{
    return input.gamepad.last_button_pressed;
}

int graphic_gamepad_axis_count_get(int gamepad)
{
    if (gamepad < 0 || gamepad >= MAX_GAMEPADS)
        return 0;

    return input.gamepad.axis_count[gamepad];
}

float graphic_gamepad_axis_movement_get(int gamepad,
                                        int axis)
{
    if (gamepad < 0             ||
        gamepad >= MAX_GAMEPADS ||
        axis    < 0             ||
        axis    >= MAX_GAMEPAD_AXES)
        return 0.0f;

    return input.gamepad.axis[gamepad][axis];
}

int graphic_gamepad_mappings_set(const char *mappings)
{
    if (mappings == NULL)
        return 0;

    return glfwUpdateGamepadMappings(mappings);
}

/*
 * Not implemented: GLFW exposes no rumble interface at all. Kept in
 * the API so that a future SDL or platform specific backend can fill
 * it in without changing any calling code.
 */
void graphic_gamepad_vibration_set(int   gamepad,
                                   float left_motor,
                                   float right_motor,
                                   float duration)
{
    (void)gamepad;
    (void)left_motor;
    (void)right_motor;
    (void)duration;
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

/* ------------------------------------------------------------------ */
/* touch                                                               */
/* ------------------------------------------------------------------ */

vec2_t graphic_touch_position_get(int index)
{
    vec2_t position = { 0.0f, 0.0f };

    if (index < 0 || index >= MAX_TOUCH_POINTS)
        return position;

    return input.touch.position[index];
}

int graphic_touch_x_get(void)
{
    return (int)input.touch.position[0].x;
}

int graphic_touch_y_get(void)
{
    return (int)input.touch.position[0].y;
}

int graphic_touch_point_id_get(int index)
{
    if (index < 0 || index >= MAX_TOUCH_POINTS)
        return -1;

    return input.touch.id[index];
}

int graphic_touch_point_count_get(void)
{
    return input.touch.count;
}

/* ------------------------------------------------------------------ */
/* injection, for automation playback                                 */
/*                                                                    */
/* These write into the same state the callbacks fill. They are set   */
/* after the poll and before the frame reads anything, which is what  */
/* keeps pressed and released working across a replay.                */
/* ------------------------------------------------------------------ */

void graphic_input_key_inject(int  key,
                              bool down)
{
    if (key < 0 || key >= MAX_KEYBOARD_KEYS)
        return;

    input.keyboard.current[key] = down ? 1 : 0;

    if (down && input.keyboard.key_queue_count < MAX_KEY_PRESSED_QUEUE)
        input.keyboard.key_queue[input.keyboard.key_queue_count++] = key;
}

void graphic_input_mouse_button_inject(int  button,
                                       bool down)
{
    if (button < 0 || button >= MAX_MOUSE_BUTTONS)
        return;

    input.mouse.current[button] = down ? 1 : 0;
}

void graphic_input_mouse_position_inject(vec2_t position)
{
    input.mouse.position = position;
}

void graphic_input_mouse_wheel_inject(vec2_t motion)
{
    input.mouse.wheel = motion;
}