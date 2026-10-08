#ifndef GRAPHIC_BACKEND_H
#define GRAPHIC_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

#include "graphic_internal.h"

bool graphic_backend_init(int         width,
                          int         height,
                          const char *title,
                          uint32_t    flags);

void graphic_backend_shutdown(void);

/*
 * Returns false when the frame must be skipped, either because the
 * window is minimised or because the swapchain went out of date.
 */
bool graphic_backend_frame_begin(void);
void graphic_backend_frame_end(void);

void graphic_backend_clear_color_set(graphic_color_t color);
bool graphic_backend_swapchain_recreate(void);

#endif /* GRAPHIC_BACKEND_H */