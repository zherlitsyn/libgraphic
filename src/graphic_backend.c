#define GRAPHIC_INVALID_QUEUE_FAMILY    UINT32_MAX

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
    VkSurfaceKHR surface;

    VkPhysicalDevice physical_device;
    uint32_t graphics_family;
    uint32_t present_family;

    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
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
/* physical device                                                    */
/* ------------------------------------------------------------------ */

static bool queue_families_find(VkPhysicalDevice device, uint32_t *graphics,
                uint32_t *present)
{
    VkQueueFamilyProperties families[16];
    uint32_t count = 16;

    *graphics = GRAPHIC_INVALID_QUEUE_FAMILY;
    *present  = GRAPHIC_INVALID_QUEUE_FAMILY;

    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families);

    for (uint32_t i = 0; i < count; i++) {
        VkBool32 supports_present = VK_FALSE;

        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            *graphics == GRAPHIC_INVALID_QUEUE_FAMILY)
            *graphics = i;

        vkGetPhysicalDeviceSurfaceSupportKHR(device, i,
                                             backend.surface,
                                             &supports_present);

        if (supports_present && *present == GRAPHIC_INVALID_QUEUE_FAMILY)
            *present = i;

        /* prefer a single family that can do both */
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            supports_present) {
            *graphics = i;
            *present = i;
            break;
        }
    }

    return *graphics != GRAPHIC_INVALID_QUEUE_FAMILY &&
           *present  != GRAPHIC_INVALID_QUEUE_FAMILY;
}

static bool device_suitable(VkPhysicalDevice device, uint32_t *graphics,
                            uint32_t *present)
{
    VkPhysicalDeviceVulkan13Features features13 = { 0 };
    VkPhysicalDeviceFeatures2 features = { 0 };
    VkPhysicalDeviceProperties properties;

    vkGetPhysicalDeviceProperties(device, &properties);

    if (properties.apiVersion < VK_API_VERSION_1_3)
        return false;

    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext   = &features13;

    vkGetPhysicalDeviceFeatures2(device, &features);

    if (!features13.dynamicRendering || !features13.synchronization2)
        return false;

    return queue_families_find(device, graphics, present);
}

static bool physical_device_pick(void)
{
    VkPhysicalDeviceProperties properties;
    VkPhysicalDevice devices[16];
    uint32_t count = 16;
    int best_score = -1;

    if (vkEnumeratePhysicalDevices(backend.instance,
                                   &count,
                                   devices) != VK_SUCCESS || count == 0)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t graphics;
        uint32_t present;

        if (!device_suitable(devices[i], &graphics, &present))
            continue;

        vkGetPhysicalDeviceProperties(devices[i], &properties);

        int score = 1;
        if (properties.deviceType ==
            VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            score = 100;
        else if (properties.deviceType ==
                 VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            score = 50;

        if (score > best_score) {
            best_score = score;
            backend.physical_device = devices[i];
            backend.graphics_family = graphics;
            backend.present_family = present;
        }
    }

    if (best_score < 0) {
        fprintf(stderr, "[graphic] no device with Vulkan 1.3 core "
                        "dynamic rendering found\n");
        return false;
    }

    vkGetPhysicalDeviceProperties(backend.physical_device,
                                  &properties);

    fprintf(stderr, "[graphic] device: %s, queue families: "
                    "graphics %u, present %u\n",
                    properties.deviceName,
                    backend.graphics_family,
                    backend.present_family);

    return true;
}

/* ------------------------------------------------------------------ */
/* logical device                                                     */
/* ------------------------------------------------------------------ */

static bool device_create(void)
{
    VkDeviceQueueCreateInfo queues[2] = { { 0 } };
    uint32_t queue_count = 1;
    const float priority = 1.0f;

    queues[0].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queues[0].queueFamilyIndex = backend.graphics_family;
    queues[0].queueCount       = 1;
    queues[0].pQueuePriorities = &priority;

    if (backend.present_family != backend.graphics_family) {
        queues[1]                  = queues[0];
        queues[1].queueFamilyIndex = backend.present_family;
        queue_count                = 2;
    }

    VkPhysicalDeviceVulkan13Features features13 = { 0 };
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceFeatures2 features = { 0 };
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features13;

    const char *extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo info = { 0 };
    info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.pNext                   = &features; /* pEnabledFeatures stays NULL */
    info.queueCreateInfoCount    = queue_count;
    info.pQueueCreateInfos       = queues;
    info.enabledExtensionCount   = 1;
    info.ppEnabledExtensionNames = extensions;

    if (vkCreateDevice(backend.physical_device, &info, NULL,
                       &backend.device) != VK_SUCCESS) {
        fprintf(stderr, "[graphic] vkCreateDevice failed\n");
        return false;
    }

    vkGetDeviceQueue(backend.device, backend.graphics_family, 0,
                     &backend.graphics_queue);
    vkGetDeviceQueue(backend.device, backend.present_family, 0,
                     &backend.present_queue);

    return true;
}

/* ------------------------------------------------------------------ */
/* initialisation                                                     */
/* ------------------------------------------------------------------ */

 bool graphic_backend_init(int width, int height, const char *title,
                           uint32_t flags)
 {
    backend.validation = (flags & GRAPHIC_WINDOW_VALIDATION) != 0;

    if (!graphic_platform_window_create(width, height, title, flags))
        return false;
 
    if (!instance_create(title))
        return false;

    if (!graphic_platform_surface_create(backend.instance, &backend.surface))
        return false;

    if (!physical_device_pick())
        return false;

    if (!device_create())
        return false;

    return true;
 }

/* ------------------------------------------------------------------ */
/* shutdown                                                           */
/* ------------------------------------------------------------------ */

void graphic_backend_shutdown(void)
{
    if (backend.device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(backend.device);

    if (backend.device != VK_NULL_HANDLE)
        vkDestroyDevice(backend.device, NULL);

    if (backend.surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(backend.instance, backend.surface, NULL);

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
