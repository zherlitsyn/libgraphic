#include <stddef.h>
#include <stdio.h>

#include "graphic_internal.h"
#include "graphic_backend.h"
#include "graphic_platform.h"

/*
 * Frame orchestration. This file owns nothing: it only sequences the
 * batch layer and the backend, and keeps the camera bookkeeping that
 * neither of them should know about.
 */

struct graphic_core_state {
    uint32_t flags;
    bool initialised;
    bool frame_valid;   /* false when the frame was skipped */
};

static struct graphic_core_state core;

void graphic_window_flags_set(uint32_t flags)
{
    core.flags = flags;
}

bool graphic_window(int width, int height, const char *title)
{
    if (!graphic_backend_init(width, height, title, core.flags))
        return false;

    core.initialised = true;

    return true;
}

void graphic_window_close(void)
{
    if (!core.initialised)
        return;

    graphic_backend_shutdown();
    core.initialised = false;
}

void graphic_drawing_begin(void)
{
    core.frame_valid = graphic_backend_frame_begin();
}

/*
 * Only remembered here: the clear happens when the frame is recorded,
 * as the first thing done to the window image.
 */
void graphic_background_clear(graphic_color_t color)
{
    graphic_backend_clear_color_set(color);
}

/*
 * Submits and presents the frame, if one was begun.
 */
void graphic_screen_buffer_swap(void)
{
    if (core.frame_valid) {
        graphic_backend_frame_end();
        core.frame_valid = false;
    }
}

void graphic_drawing_end(void)
{
    graphic_screen_buffer_swap();
    graphic_events_poll();
}
