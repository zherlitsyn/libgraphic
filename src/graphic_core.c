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
}

void graphic_drawing_end(void)
{
    graphic_events_poll();
}
