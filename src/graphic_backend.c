#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>

#include "graphic_internal.h"
#include "graphic_backend.h"
#include "graphic_platform.h"

/* ------------------------------------------------------------------ */
/* initialisation                                                     */
/* ------------------------------------------------------------------ */

bool graphic_backend_init(int width, int height, const char *title, uint32_t flags)
{
    if (!graphic_platform_window_create(width, height, title, flags))
        return false;

    return true;
}

/* ------------------------------------------------------------------ */
/* shutdown                                                           */
/* ------------------------------------------------------------------ */

void graphic_backend_shutdown(void)
{
    graphic_platform_window_destroy();
}
