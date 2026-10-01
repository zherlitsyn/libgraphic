#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>

#include "graphic_internal.h"
#include "graphic_backend.h"
#include "graphic_platform.h"

struct graphic_backend {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    bool validation;
};

static struct graphic_backend backend;

/* ------------------------------------------------------------------ */
/* instance                                                            */
/* ------------------------------------------------------------------ */

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT types,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *data,
                                                     void *user_data)
{
    (void)types;
    (void)user_data;

    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        fprintf(stderr, "[vulkan] %s\n", data->pMessage);

    return VK_FALSE;
}

static bool validation_layer_available(void)
{
    VkLayerProperties layers[64];
    uint32_t count = 64;
    uint32_t i;

    if (vkEnumerateInstanceLayerProperties(&count, layers) != VK_SUCCESS)
        return false;

    for (i = 0; i < count; i++) {
        if (strcmp(layers[i].layerName, "VK_LAYER_KHRONOS_validation") == 0)
            return true;
    }

    return false;
}

static bool instance_create(const char *title)
{
    uint32_t api_version = 0;
    if (vkEnumerateInstanceVersion(&api_version) != VK_SUCCESS ||
        api_version < VK_API_VERSION_1_3) {
        fprintf(stderr, "[graphic] loader reports no Vulkan 1.3\n");
        return false;
    }

    const char *const *platform_extensions;
    uint32_t platform_extension_count = 0;
    platform_extensions = 
        graphic_platform_instance_extensions_get(&platform_extension_count);

    if (platform_extensions == NULL)
        return false;

    const char *extensions[16];
    uint32_t count = 0;
    for (uint32_t i = 0; i < platform_extension_count && count < 15; i++)
        extensions[count++] = platform_extensions[i];

    if (backend.validation && validation_layer_available())
        extensions[count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    else
        backend.validation = false;

    VkApplicationInfo application = { 0 };
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = title;
    application.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    application.pEngineName = "graphic";
    application.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo info = { 0 };
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &application;
    info.enabledExtensionCount = count;
    info.ppEnabledExtensionNames = extensions;

    if (backend.validation) {
        const char *layer = "VK_LAYER_KHRONOS_validation";
        info.enabledLayerCount = 1;
        info.ppEnabledLayerNames = &layer;
    }

    if (vkCreateInstance(&info, NULL, &backend.instance) != VK_SUCCESS) {
        fprintf(stderr, "[graphic] vkCreateInstance failed\n");
        return false;
    }

    if (backend.validation) {
        PFN_vkCreateDebugUtilsMessengerEXT create;
        VkDebugUtilsMessengerCreateInfoEXT debug = { 0 };

        debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        debug.pfnUserCallback = debug_callback;

        create = (PFN_vkCreateDebugUtilsMessengerEXT)
                     vkGetInstanceProcAddr(backend.instance,
                                           "vkCreateDebugUtilsMessengerEXT");
        if (create != NULL)
            create(backend.instance, &debug, NULL, &backend.debug_messenger);
    }

    fprintf(stderr, "[graphic] instance: API %u.%u, %u extensions, validation %s\n",
                    VK_API_VERSION_MAJOR(api_version),
                    VK_API_VERSION_MINOR(api_version),
                    count,
                    backend.validation ? "on" : "off");

    return true;
}

/* ------------------------------------------------------------------ */
/* initialisation                                                     */
/* ------------------------------------------------------------------ */

 bool graphic_backend_init(int width, int height, const char *title, uint32_t flags)
 {
    backend.validation = (flags & GRAPHIC_WINDOW_VALIDATION) != 0;

    if (!graphic_platform_window_create(width, height, title, flags))
        return false;
 
    if (!instance_create(title))
        return false;

    return true;
 }

/* ------------------------------------------------------------------ */
/* shutdown                                                           */
/* ------------------------------------------------------------------ */

void graphic_backend_shutdown(void)
{
    if (backend.debug_messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy;

        destroy = (PFN_vkDestroyDebugUtilsMessengerEXT)
                      vkGetInstanceProcAddr(backend.instance,
                                           "vkDestroyDebugUtilsMessengerEXT");
        if (destroy != NULL)
            destroy(backend.instance, backend.debug_messenger, NULL);
    }

    if (backend.instance != VK_NULL_HANDLE)
        vkDestroyInstance(backend.instance, NULL);

    graphic_platform_window_destroy();

    memset(&backend, 0, sizeof(backend));
}
