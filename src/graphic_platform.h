#ifndef GRAPHIC_PLATFORM_H
#define GRAPHIC_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include <vulkan/vulkan.h>

#include "graphic.h"

/*
 * Internal hooks the rest of the library needs from the platform.
 * The user facing window, monitor and cursor calls are declared in
 * graphic.h and implemented in the same file as these, because on the
 * desktop they are nothing but thin GLFW wrappers.
 *
 * Porting to another window system means writing one more
 * graphic_platform_*.c that implements both sets.
 */

bool graphic_platform_window_create(int width, int height, const char *title,
                                    uint32_t flags);
void graphic_platform_window_destroy(void);
const char *const *graphic_platform_instance_extensions_get(uint32_t *count);
bool graphic_platform_surface_create(VkInstance instance,
                                     VkSurfaceKHR *surface);

#endif /* GRAPHIC_PLATFORM_H */