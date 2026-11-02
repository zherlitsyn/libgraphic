#ifndef GRAPHIC_INPUT_H
#define GRAPHIC_INPUT_H

#include <stdbool.h>

#include "graphic_linalg.h"

/*
 * Keyboard, mouse, gamepad and touch.
 *
 * Key and button values match GLFW, so the tables can be indexed
 * directly with no translation on the desktop.
 */

enum graphic_key {
    GRAPHIC_KEY_NULL                   = 0,

    /* printable */
    GRAPHIC_KEY_SPACE                  = 32,
    GRAPHIC_KEY_APOSTROPHE             = 39,
    GRAPHIC_KEY_COMMA                  = 44,
    GRAPHIC_KEY_MINUS                  = 45,
    GRAPHIC_KEY_PERIOD                 = 46,
    GRAPHIC_KEY_SLASH                  = 47,
    GRAPHIC_KEY_ZERO                   = 48,
    GRAPHIC_KEY_ONE                    = 49,
    GRAPHIC_KEY_TWO                    = 50,
    GRAPHIC_KEY_THREE                  = 51,
    GRAPHIC_KEY_FOUR                   = 52,
    GRAPHIC_KEY_FIVE                   = 53,
    GRAPHIC_KEY_SIX                    = 54,
    GRAPHIC_KEY_SEVEN                  = 55,
    GRAPHIC_KEY_EIGHT                  = 56,
    GRAPHIC_KEY_NINE                   = 57,
    GRAPHIC_KEY_SEMICOLON              = 59,
    GRAPHIC_KEY_EQUAL                  = 61,
    GRAPHIC_KEY_A                      = 65,
    GRAPHIC_KEY_B                      = 66,
    GRAPHIC_KEY_C                      = 67,
    GRAPHIC_KEY_D                      = 68,
    GRAPHIC_KEY_E                      = 69,
    GRAPHIC_KEY_F                      = 70,
    GRAPHIC_KEY_G                      = 71,
    GRAPHIC_KEY_H                      = 72,
    GRAPHIC_KEY_I                      = 73,
    GRAPHIC_KEY_J                      = 74,
    GRAPHIC_KEY_K                      = 75,
    GRAPHIC_KEY_L                      = 76,
    GRAPHIC_KEY_M                      = 77,
    GRAPHIC_KEY_N                      = 78,
    GRAPHIC_KEY_O                      = 79,
    GRAPHIC_KEY_P                      = 80,
    GRAPHIC_KEY_Q                      = 81,
    GRAPHIC_KEY_R                      = 82,
    GRAPHIC_KEY_S                      = 83,
    GRAPHIC_KEY_T                      = 84,
    GRAPHIC_KEY_U                      = 85,
    GRAPHIC_KEY_V                      = 86,
    GRAPHIC_KEY_W                      = 87,
    GRAPHIC_KEY_X                      = 88,
    GRAPHIC_KEY_Y                      = 89,
    GRAPHIC_KEY_Z                      = 90,
    GRAPHIC_KEY_LEFT_BRACKET           = 91,
    GRAPHIC_KEY_BACKSLASH              = 92,
    GRAPHIC_KEY_RIGHT_BRACKET          = 93,
    GRAPHIC_KEY_GRAVE                  = 96,

    /* function */
    GRAPHIC_KEY_ESCAPE                 = 256,
    GRAPHIC_KEY_ENTER                  = 257,
    GRAPHIC_KEY_TAB                    = 258,
    GRAPHIC_KEY_BACKSPACE              = 259,
    GRAPHIC_KEY_INSERT                 = 260,
    GRAPHIC_KEY_DELETE                 = 261,
    GRAPHIC_KEY_RIGHT                  = 262,
    GRAPHIC_KEY_LEFT                   = 263,
    GRAPHIC_KEY_DOWN                   = 264,
    GRAPHIC_KEY_UP                     = 265,
    GRAPHIC_KEY_PAGE_UP                = 266,
    GRAPHIC_KEY_PAGE_DOWN              = 267,
    GRAPHIC_KEY_HOME                   = 268,
    GRAPHIC_KEY_END                    = 269,
    GRAPHIC_KEY_CAPS_LOCK              = 280,
    GRAPHIC_KEY_SCROLL_LOCK            = 281,
    GRAPHIC_KEY_NUM_LOCK               = 282,
    GRAPHIC_KEY_PRINT_SCREEN           = 283,
    GRAPHIC_KEY_PAUSE                  = 284,
    GRAPHIC_KEY_F1                     = 290,
    GRAPHIC_KEY_F2                     = 291,
    GRAPHIC_KEY_F3                     = 292,
    GRAPHIC_KEY_F4                     = 293,
    GRAPHIC_KEY_F5                     = 294,
    GRAPHIC_KEY_F6                     = 295,
    GRAPHIC_KEY_F7                     = 296,
    GRAPHIC_KEY_F8                     = 297,
    GRAPHIC_KEY_F9                     = 298,
    GRAPHIC_KEY_F10                    = 299,
    GRAPHIC_KEY_F11                    = 300,
    GRAPHIC_KEY_F12                    = 301,
    GRAPHIC_KEY_LEFT_SHIFT             = 340,
    GRAPHIC_KEY_LEFT_CONTROL           = 341,
    GRAPHIC_KEY_LEFT_ALT               = 342,
    GRAPHIC_KEY_LEFT_SUPER             = 343,
    GRAPHIC_KEY_RIGHT_SHIFT            = 344,
    GRAPHIC_KEY_RIGHT_CONTROL          = 345,
    GRAPHIC_KEY_RIGHT_ALT              = 346,
    GRAPHIC_KEY_RIGHT_SUPER            = 347,
    GRAPHIC_KEY_MENU                   = 348,

    /* keypad */
    GRAPHIC_KEY_KP_0                   = 320,
    GRAPHIC_KEY_KP_1                   = 321,
    GRAPHIC_KEY_KP_2                   = 322,
    GRAPHIC_KEY_KP_3                   = 323,
    GRAPHIC_KEY_KP_4                   = 324,
    GRAPHIC_KEY_KP_5                   = 325,
    GRAPHIC_KEY_KP_6                   = 326,
    GRAPHIC_KEY_KP_7                   = 327,
    GRAPHIC_KEY_KP_8                   = 328,
    GRAPHIC_KEY_KP_9                   = 329,
    GRAPHIC_KEY_KP_DECIMAL             = 330,
    GRAPHIC_KEY_KP_DIVIDE              = 331,
    GRAPHIC_KEY_KP_MULTIPLY            = 332,
    GRAPHIC_KEY_KP_SUBTRACT            = 333,
    GRAPHIC_KEY_KP_ADD                 = 334,
    GRAPHIC_KEY_KP_ENTER               = 335,
    GRAPHIC_KEY_KP_EQUAL               = 336,
};

enum graphic_mouse_button {
    GRAPHIC_MOUSE_BUTTON_LEFT          = 0,
    GRAPHIC_MOUSE_BUTTON_RIGHT         = 1,
    GRAPHIC_MOUSE_BUTTON_MIDDLE        = 2,
    GRAPHIC_MOUSE_BUTTON_SIDE          = 3,
    GRAPHIC_MOUSE_BUTTON_EXTRA         = 4,
    GRAPHIC_MOUSE_BUTTON_FORWARD       = 5,
    GRAPHIC_MOUSE_BUTTON_BACK          = 6,
};

enum graphic_mouse_cursor {
    GRAPHIC_MOUSE_CURSOR_DEFAULT       = 0,
    GRAPHIC_MOUSE_CURSOR_ARROW         = 1,
    GRAPHIC_MOUSE_CURSOR_IBEAM         = 2,
    GRAPHIC_MOUSE_CURSOR_CROSSHAIR     = 3,
    GRAPHIC_MOUSE_CURSOR_POINTING_HAND = 4,
    GRAPHIC_MOUSE_CURSOR_RESIZE_EW     = 5,
    GRAPHIC_MOUSE_CURSOR_RESIZE_NS     = 6,
    GRAPHIC_MOUSE_CURSOR_RESIZE_NWSE   = 7,
    GRAPHIC_MOUSE_CURSOR_RESIZE_NESW   = 8,
    GRAPHIC_MOUSE_CURSOR_RESIZE_ALL    = 9,
    GRAPHIC_MOUSE_CURSOR_NOT_ALLOWED   = 10,
};

/* Laid out as seen on the pad, not as the driver numbers them */
enum graphic_gamepad_button {
    GRAPHIC_GAMEPAD_BUTTON_UNKNOWN = 0,
    GRAPHIC_GAMEPAD_BUTTON_LEFT_FACE_UP,     /* d-pad        */
    GRAPHIC_GAMEPAD_BUTTON_LEFT_FACE_RIGHT,
    GRAPHIC_GAMEPAD_BUTTON_LEFT_FACE_DOWN,
    GRAPHIC_GAMEPAD_BUTTON_LEFT_FACE_LEFT,
    GRAPHIC_GAMEPAD_BUTTON_RIGHT_FACE_UP,    /* Y / triangle */
    GRAPHIC_GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, /* B / circle   */
    GRAPHIC_GAMEPAD_BUTTON_RIGHT_FACE_DOWN,  /* A / cross    */
    GRAPHIC_GAMEPAD_BUTTON_RIGHT_FACE_LEFT,  /* X / square   */
    GRAPHIC_GAMEPAD_BUTTON_LEFT_TRIGGER_1,   /* bumper       */
    GRAPHIC_GAMEPAD_BUTTON_LEFT_TRIGGER_2,   /* analogue     */
    GRAPHIC_GAMEPAD_BUTTON_RIGHT_TRIGGER_1,
    GRAPHIC_GAMEPAD_BUTTON_RIGHT_TRIGGER_2,
    GRAPHIC_GAMEPAD_BUTTON_MIDDLE_LEFT,      /* select       */
    GRAPHIC_GAMEPAD_BUTTON_MIDDLE,           /* guide        */
    GRAPHIC_GAMEPAD_BUTTON_MIDDLE_RIGHT,     /* start        */
    GRAPHIC_GAMEPAD_BUTTON_LEFT_THUMB,
    GRAPHIC_GAMEPAD_BUTTON_RIGHT_THUMB,
};

enum graphic_gamepad_axis {
    GRAPHIC_GAMEPAD_AXIS_LEFT_X        = 0,
    GRAPHIC_GAMEPAD_AXIS_LEFT_Y        = 1,
    GRAPHIC_GAMEPAD_AXIS_RIGHT_X       = 2,
    GRAPHIC_GAMEPAD_AXIS_RIGHT_Y       = 3,
    GRAPHIC_GAMEPAD_AXIS_LEFT_TRIGGER  = 4,
    GRAPHIC_GAMEPAD_AXIS_RIGHT_TRIGGER = 5,
};

/* ------------------------------------------------------------------ */
/* input callbacks                                                    */
/*                                                                    */
/* The predicates below answer "what is the state now". A caller that */
/* drives a user interface needs more than that: the order events     */
/* happened in, and the events a once per frame snapshot cannot see - */
/* a click that opened and closed between two frames, or a move that  */
/* landed after a button went down.                                   */
/*                                                                    */
/* These hand every event over the moment it arrives, in the order the*/
/* window system reported it. They run after the internal state is    */
/* updated, so the predicates keep working unchanged.                 */
/*                                                                    */
/* Each is set on its own and carries its own context, so a caller    */
/* that wants one event does not have to name the other four.         */
/*                                                                    */
/* Rules:                                                             */
/*                                                                    */
/*   - a callback may be called outside your frame. GLFW fires them   */
/*     from graphic_events_poll(), but on some platforms also from    */
/*     the modal loop the system runs while a window is dragged or    */
/*     resized;                                                       */
/*   - therefore a callback must only record what happened. No        */
/*     drawing, no window creation or destruction, no waiting;        */
/*   - they run on the thread that pumps events, which is the thread  */
/*     that created the window.                                       */
/*                                                                    */
/* Pass NULL to stop receiving that event.                            */
/* ------------------------------------------------------------------ */

typedef void (*graphic_mouse_move_callback_t)(void *context,
                                              float x,
                                              float y);

typedef void (*graphic_mouse_button_callback_t)(void *context,
                                                int   button,
                                                bool  down,
                                                float x,
                                                float y);

typedef void (*graphic_scroll_callback_t)(void *context,
                                          float x,
                                          float y);

typedef void (*graphic_key_callback_t)(void *context,
                                       int   key,
                                       bool  down,
                                       bool  repeat);

/* unicode, already composed by the system */
typedef void (*graphic_text_callback_t)(void *context,
                                        int   codepoint);

void graphic_mouse_move_callback_set(graphic_mouse_move_callback_t callback,
                                     void                         *context);

void graphic_mouse_button_callback_set(graphic_mouse_button_callback_t callback,
                                       void                           *context);

void graphic_scroll_callback_set(graphic_scroll_callback_t callback,
                                 void                     *context);

void graphic_key_callback_set(graphic_key_callback_t callback,
                              void                  *context);

void graphic_text_callback_set(graphic_text_callback_t callback,
                               void                   *context);

/* forgets all five at once, for a caller shutting a screen down */
void graphic_input_callbacks_clear(void);

/* ------------------------------------------------------------------ */
/* keyboard                                                           */
/* ------------------------------------------------------------------ */

bool graphic_is_key_pressed(int key);        /* went down this frame */

bool graphic_is_key_pressed_repeat(int key); /* auto repeat fired */

bool graphic_is_key_down(int key);

bool graphic_is_key_released(int key);

bool graphic_is_key_up(int key);

int graphic_key_pressed_get(void);           /* queue, 0 when empty */

int graphic_char_pressed_get(void);          /* unicode, 0 when empty */

const char *graphic_key_name_get(int key);

void graphic_exit_key_set(int key);          /* 0 disables */

/* ------------------------------------------------------------------ */
/* gamepad                                                            */
/* ------------------------------------------------------------------ */

bool graphic_is_gamepad_available(int gamepad);

const char *graphic_gamepad_name_get(int gamepad);

bool graphic_is_gamepad_button_pressed(int gamepad,
                                       int button);

bool graphic_is_gamepad_button_down(int gamepad,
                                    int button);

bool graphic_is_gamepad_button_released(int gamepad,
                                        int button);

bool graphic_is_gamepad_button_up(int gamepad,
                                  int button);

int graphic_gamepad_button_pressed_get(void);

int graphic_gamepad_axis_count_get(int gamepad);

float graphic_gamepad_axis_movement_get(int gamepad,
                                        int axis);

int graphic_gamepad_mappings_set(const char *mappings);

void graphic_gamepad_vibration_set(int   gamepad,
                                   float left_motor,
                                   float right_motor,
                                   float duration);

/* ------------------------------------------------------------------ */
/* mouse                                                              */
/* ------------------------------------------------------------------ */

bool graphic_is_mouse_button_pressed(int button);

bool graphic_is_mouse_button_down(int button);

bool graphic_is_mouse_button_released(int button);

bool graphic_is_mouse_button_up(int button);

int graphic_mouse_x_get(void);

int graphic_mouse_y_get(void);

vec2_t graphic_mouse_position_get(void);

vec2_t graphic_mouse_delta_get(void);

void graphic_mouse_position_set(int x,
                                int y);

void graphic_mouse_offset_set(int offset_x,
                              int offset_y);

void graphic_mouse_scale_set(float scale_x,
                             float scale_y);

float graphic_mouse_wheel_move_get(void);    /* largest of the two axes */

vec2_t graphic_mouse_wheel_move_v_get(void);

void graphic_mouse_cursor_set(int cursor);

/* ------------------------------------------------------------------ */
/* touch                                                              */
/*                                                                    */
/* On the desktop there is no touch device, so point 0 mirrors the    */
/* mouse and the count is 1 while a button is held.                   */
/* ------------------------------------------------------------------ */

int graphic_touch_x_get(void);

int graphic_touch_y_get(void);

vec2_t graphic_touch_position_get(int index);

int graphic_touch_point_id_get(int index);

int graphic_touch_point_count_get(void);

#endif /* GRAPHIC_INPUT_H */