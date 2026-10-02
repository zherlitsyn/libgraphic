#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

/*
 * Mouse passthrough, a window that clicks fall through, arrived in
 * GLFW 3.4 and the project asks for 3.3. Naming it here keeps the
 * calls compiling; on 3.3 glfwWindowHint and glfwGetWindowAttrib
 * ignore an attribute they do not know, so the flag simply has no
 * effect rather than breaking the build.
 */
#ifndef GLFW_MOUSE_PASSTHROUGH
#define GLFW_MOUSE_PASSTHROUGH    0x0002000D
#endif

#include <stdlib.h>
#include <string.h>

#include "graphic_platform.h"

/*
 * Desktop platform layer. The only file in the library that knows GLFW
 * exists. It carries both the internal hooks from graphic_platform.h
 * and the public window, monitor, cursor and time API, since on the
 * desktop those are one line wrappers and an extra indirection would
 * only add code without adding a seam.
 */

struct graphic_window_state {
    GLFWwindow *handle;
    uint32_t flags;            /* requested at creation time */

    double time_start;
};

static struct graphic_window_state window;

/* ------------------------------------------------------------------ */
/* creation and teardown                                              */
/* ------------------------------------------------------------------ */

bool graphic_platform_window_create(int width, int height, const char *title,
                                    uint32_t flags)
{
    GLFWmonitor *monitor = NULL;

    if (!glfwInit()) {
        return false;
    }

    if (!glfwVulkanSupported()) {
        glfwTerminate();
        return false;
    }

    /* GLFW must not create any OpenGL context for us */
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    glfwWindowHint(GLFW_RESIZABLE,
                   (flags & GRAPHIC_WINDOW_RESIZABLE) ? 
                    GLFW_TRUE : GLFW_FALSE);

    glfwWindowHint(GLFW_DECORATED,
                   (flags & GRAPHIC_WINDOW_UNDECORATED) ? 
                    GLFW_FALSE : GLFW_TRUE);
                  
    glfwWindowHint(GLFW_VISIBLE,
                   (flags & GRAPHIC_WINDOW_HIDDEN) ?
                    GLFW_FALSE : GLFW_TRUE);

    glfwWindowHint(GLFW_MAXIMIZED,
                   (flags & GRAPHIC_WINDOW_MAXIMIZED) ? 
                    GLFW_TRUE : GLFW_FALSE);

    glfwWindowHint(GLFW_FOCUSED,
                   (flags & GRAPHIC_WINDOW_UNFOCUSED) ?
                    GLFW_FALSE : GLFW_TRUE);

    glfwWindowHint(GLFW_FLOATING,
                   (flags & GRAPHIC_WINDOW_TOPMOST) ?
                    GLFW_TRUE : GLFW_FALSE);

    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER,
                   (flags & GRAPHIC_WINDOW_TRANSPARENT) ?
                    GLFW_TRUE : GLFW_FALSE);

    glfwWindowHint(GLFW_SCALE_TO_MONITOR,
                   (flags & GRAPHIC_WINDOW_HIGHDPI) ?
                    GLFW_TRUE : GLFW_FALSE);

    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH,
                   (flags & GRAPHIC_WINDOW_MOUSE_PASSTHROUGH) ?
                    GLFW_TRUE : GLFW_FALSE);

    if (flags & GRAPHIC_WINDOW_FULLSCREEN)
        monitor = glfwGetPrimaryMonitor();

    window.handle = glfwCreateWindow(width, height, title, monitor, NULL);

    if (window.handle == NULL) {
        glfwTerminate();
        return false;
    }

    window.flags = flags;
    window.time_start = glfwGetTime();

    if (flags & GRAPHIC_WINDOW_MINIMIZED)
        glfwIconifyWindow(window.handle);

    return true;
}

void graphic_platform_window_destroy(void)
{
    if (window.handle != NULL) {
        glfwDestroyWindow(window.handle);
        window.handle = NULL;
    }

    glfwTerminate();
}

/* ------------------------------------------------------------------ */
/* Vulkan facing hooks                                                */
/* ------------------------------------------------------------------ */

const char *const *graphic_platform_instance_extensions_get(uint32_t *count)
{
    return glfwGetRequiredInstanceExtensions(count);
}

bool graphic_platform_surface_create(VkInstance instance,
                                     VkSurfaceKHR *surface)
{
    return glfwCreateWindowSurface(instance, window.handle, NULL,
                                   surface) == VK_SUCCESS;
}

void graphic_platform_framebuffer_size_get(uint32_t *width, uint32_t *height)
{
    int w = 0;
    int h = 0;

    if (window.handle != NULL)
        glfwGetFramebufferSize(window.handle, &w, &h);

    *width  = (uint32_t)w;
    *height = (uint32_t)h;
}

/* ------------------------------------------------------------------ */
/* window state queries                                               */
/* ------------------------------------------------------------------ */

bool graphic_window_should_close(void)
{
    if (window.handle == NULL)
        return true;

    return glfwWindowShouldClose(window.handle) != 0;
}

/* ------------------------------------------------------------------ */
/* events, cursor and time                                            */
/* ------------------------------------------------------------------ */

/*
 * The one place per frame where the window system is pumped.
 */
void graphic_events_poll(void)
{
    glfwPollEvents();
}

double graphic_time_get(void)
{
    return glfwGetTime() - window.time_start;
}