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
    bool     initialised;
    bool     frame_valid;     /* false when the frame was skipped */

    double   time_previous;
    double   frame_target;    /* seconds per frame, 0 disables */
    float    frame_time;
    float    fps_accumulator;
    uint32_t fps_frames;
    int      fps;
};

static struct graphic_core_state core;

void graphic_window_flags_set(uint32_t flags)
{
    core.flags = flags;
}

bool graphic_window(int         width,
                    int         height,
                    const char *title)
{
    if (!graphic_backend_init(width, height, title, core.flags))
        return false;

    core.time_previous = graphic_time_get();
    core.initialised   = true;

    return true;
}

void graphic_window_close(void)
{
    if (!core.initialised)
        return;

    graphic_backend_shutdown();
    core.initialised = false;
}

/*
 * graphic_window_should_close() lives in the platform layer, but the
 * event pump has to run somewhere every frame. Doing it here would
 * duplicate whatever the caller already does, so the loop condition
 * pumps and this file only consumes the results.
 */
void graphic_fps_target_set(int fps)
{
    core.frame_target = (fps > 0) ? 1.0 / (double)fps : 0.0;
}

float graphic_frame_time_get(void)
{
    return core.frame_time;
}

int graphic_fps_get(void)
{
    return core.fps;
}

void graphic_drawing_begin(void)
{
    double now = graphic_time_get();

    core.frame_time    = (float)(now - core.time_previous);
    core.time_previous = now;

    /* a stalled frame must not turn into a huge time step */
    if (core.frame_time > 0.25f)
        core.frame_time = 0.25f;

    core.fps_accumulator += core.frame_time;
    core.fps_frames++;

    if (core.fps_accumulator >= 0.5f) {
        core.fps = (int)((float)core.fps_frames / core.fps_accumulator + 0.5f);
        core.fps_accumulator = 0.0f;
        core.fps_frames = 0;
    }

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

    if (core.frame_target > 0.0) {
        double elapsed = graphic_time_get() - core.time_previous;

        if (elapsed < core.frame_target)
            graphic_time_wait(core.frame_target - elapsed);
    }
}
