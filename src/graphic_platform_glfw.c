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
    uint32_t    flags;   /* requested at creation time */
    bool        resized; /* set by the callback, cleared per frame */

    /* remembered across a fullscreen or borderless round trip */
    int         windowed_x;
    int         windowed_y;
    int         windowed_width;
    int         windowed_height;

    double time_start;
};

static struct graphic_window_state window;

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static GLFWmonitor *monitor_get(int index)
{
    int count = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&count);

    if (monitors == NULL || index < 0 || index >= count)
        return NULL;

    return monitors[index];
}

static void framebuffer_size_callback(GLFWwindow *handle,
                                      int         width,
                                      int         height)
{
    (void)handle;
    (void)width;
    (void)height;

    /*
     * Nothing is resized here on purpose. The swapchain notices by
     * itself through VK_ERROR_OUT_OF_DATE_KHR; this flag only feeds
     * graphic_window_is_resized() for the caller.
     */
    window.resized = true;
}

static void windowed_geometry_save(void)
{
    glfwGetWindowPos(window.handle,
                     &window.windowed_x,
                     &window.windowed_y);

    glfwGetWindowSize(window.handle,
                      &window.windowed_width,
                      &window.windowed_height);
}

/* ------------------------------------------------------------------ */
/* creation and teardown                                              */
/* ------------------------------------------------------------------ */

bool graphic_platform_window_create(int         width,
                                    int         height,
                                    const char *title,
                                    uint32_t    flags)
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

    glfwSetFramebufferSizeCallback(window.handle, framebuffer_size_callback);

    if (flags & GRAPHIC_WINDOW_MINIMIZED)
        glfwIconifyWindow(window.handle);

    windowed_geometry_save();

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

void graphic_platform_framebuffer_size_get(uint32_t *width,
                                           uint32_t *height)
{
    int w = 0;
    int h = 0;

    if (window.handle != NULL)
        glfwGetFramebufferSize(window.handle, &w, &h);

    *width  = (uint32_t)w;
    *height = (uint32_t)h;
}

void graphic_platform_resized_flag_clear(void)
{
    window.resized = false;
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

bool graphic_window_is_fullscreen(void)
{
    if (window.handle == NULL)
        return false;

    return glfwGetWindowMonitor(window.handle) != NULL;
}

bool graphic_window_is_minimized(void)
{
    if (window.handle == NULL)
        return false;

    return glfwGetWindowAttrib(window.handle, GLFW_ICONIFIED) != 0;
}

bool graphic_window_is_resized(void)
{
    return window.resized;
}

/* ------------------------------------------------------------------ */
/* window state changes                                                */
/* ------------------------------------------------------------------ */

void graphic_window_fullscreen_toggle(void)
{
    if (window.handle == NULL)
        return;

    if (graphic_window_is_fullscreen()) {
        glfwSetWindowMonitor(window.handle,
                             NULL,
                             window.windowed_x,
                             window.windowed_y,
                             window.windowed_width,
                             window.windowed_height,
                             GLFW_DONT_CARE);

        window.flags &= ~(uint32_t)GRAPHIC_WINDOW_FULLSCREEN;
        return;
    }

    GLFWmonitor *monitor;
    monitor = monitor_get(graphic_monitor_current_get());
    if (monitor == NULL)
        monitor = glfwGetPrimaryMonitor();
    if (monitor == NULL)
        return;

    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    if (mode == NULL)
        return;

    windowed_geometry_save();

    glfwSetWindowMonitor(window.handle,
                         monitor,
                         0,
                         0,
                         mode->width,
                         mode->height,
                         mode->refreshRate);

    window.flags |= GRAPHIC_WINDOW_FULLSCREEN;
}

/* ------------------------------------------------------------------ */
/* window properties                                                   */
/* ------------------------------------------------------------------ */

void graphic_window_title_set(const char *title)
{
    if (window.handle != NULL && title != NULL)
        glfwSetWindowTitle(window.handle, title);
}

/* ------------------------------------------------------------------ */
/* screen and monitors                                                 */
/* ------------------------------------------------------------------ */

int graphic_screen_width_get(void)
{
    int width  = 0;
    int height = 0;

    if (window.handle != NULL)
        glfwGetWindowSize(window.handle, &width, &height);

    return width;
}

int graphic_screen_height_get(void)
{
    int width  = 0;
    int height = 0;

    if (window.handle != NULL)
        glfwGetWindowSize(window.handle, &width, &height);

    return height;
}

int graphic_render_width_get(void)
{
    uint32_t width  = 0;
    uint32_t height = 0;

    graphic_platform_framebuffer_size_get(&width, &height);

    return (int)width;
}

int graphic_render_height_get(void)
{
    uint32_t width  = 0;
    uint32_t height = 0;

    graphic_platform_framebuffer_size_get(&width, &height);

    return (int)height;
}

/*
 * GLFW only reports a monitor for a fullscreen window, so for a
 * windowed one the monitor is deduced from where the window sits.
 */
int graphic_monitor_current_get(void)
{
    if (window.handle == NULL)
        return 0;

    int count = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&count);
    if (monitors == NULL || count == 0)
        return 0;

    GLFWmonitor *current = glfwGetWindowMonitor(window.handle);

    if (current == NULL) {
        int window_x = 0;
        int window_y = 0;
        glfwGetWindowPos(window.handle, &window_x, &window_y);

        for (int i = 0; i < count; i++) {
            const GLFWvidmode *mode = glfwGetVideoMode(monitors[i]);

            if (mode == NULL)
                continue;

            int x = 0;
            int y = 0;
            glfwGetMonitorPos(monitors[i], &x, &y);

            if (window_x >= x && window_x < x + mode->width &&
                window_y >= y && window_y < y + mode->height)
                return i;
        }

        return 0;
    }

    for (int i = 0; i < count; i++) {
        if (monitors[i] == current)
            return i;
    }

    return 0;
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

void graphic_time_wait(double seconds)
{
    if (seconds <= 0.0)
        return;

    double target = glfwGetTime() + seconds;

    /*
     * Busy wait. Crude, but it is only ever used for the last
     * fraction of a millisecond of frame limiting, where a real
     * sleep would overshoot on most schedulers.
     */
    while (glfwGetTime() < target);
}