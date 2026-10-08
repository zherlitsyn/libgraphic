#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>

#include "graphic_internal.h"
#include "graphic_backend.h"
#include "graphic_platform.h"
#include "graphic_pipeline.h"

#define GRAPHIC_MAX_SWAPCHAIN_IMAGES    8
#define GRAPHIC_INVALID_QUEUE_FAMILY    UINT32_MAX

/*
 * Per frame in flight resources. Everything the CPU writes while the
 * GPU is still reading the previous frame must be duplicated here.
 */
struct graphic_frame {
    VkCommandBuffer command_buffer;
    VkSemaphore image_available;
    VkFence in_flight;
};

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

    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    uint32_t swapchain_image_count;
    VkImage swapchain_images[GRAPHIC_MAX_SWAPCHAIN_IMAGES];
    VkImageView swapchain_image_views[GRAPHIC_MAX_SWAPCHAIN_IMAGES];

    /*
     * One per swapchain image, not per frame in flight: the present
     * engine gives no signal telling us when it stopped waiting on
     * this semaphore, so it may only be reused once the image it
     * belongs to comes back from acquire.
     */
    VkSemaphore render_finished[GRAPHIC_MAX_SWAPCHAIN_IMAGES];

    VkCommandPool command_pool;
    struct graphic_frame frames[GRAPHIC_FRAMES_IN_FLIGHT];

    uint32_t frame_index;
    uint32_t image_index;
    VkClearColorValue clear_color;
    bool frame_active;
    bool swapchain_dirty;
    bool swapchain_ready; /* window image already transitioned */

    bool vsync;
    bool validation;
};

static struct graphic_backend backend;

/* ------------------------------------------------------------------ */
/* instance                                                            */
/* ------------------------------------------------------------------ */

static VKAPI_ATTR VkBool32 VKAPI_CALL
    debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
                   VkDebugUtilsMessageTypeFlagsEXT             types,
                   const VkDebugUtilsMessengerCallbackDataEXT *data,
                   void                                       *user_data)
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

static bool queue_families_find(VkPhysicalDevice device,
                                uint32_t        *graphics,
                                uint32_t        *present)
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

static bool device_suitable(VkPhysicalDevice device,
                            uint32_t        *graphics,
                            uint32_t        *present)
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
/* swapchain and depth                                                */
/* ------------------------------------------------------------------ */

static VkSurfaceFormatKHR surface_format_pick(void)
{
    VkSurfaceFormatKHR formats[32];
    uint32_t count = 32;

    vkGetPhysicalDeviceSurfaceFormatsKHR(backend.physical_device,
                                         backend.surface,
                                         &count,
                                         formats);

    for (uint32_t i = 0; i < count; i++) {
        if (formats[i].format     == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return formats[i];
    }

    return formats[0];
}

static VkPresentModeKHR present_mode_pick(void)
{
    VkPresentModeKHR modes[8];
    uint32_t count = 8;

    if (backend.vsync)
        return VK_PRESENT_MODE_FIFO_KHR; /* always supported */

    vkGetPhysicalDeviceSurfacePresentModesKHR(backend.physical_device,
                                              backend.surface,
                                              &count,
                                              modes);

    for (uint32_t i = 0; i < count; i++) {
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            return modes[i];
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static bool swapchain_create(void)
{
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(backend.physical_device,
                                              backend.surface,
                                              &capabilities);

    uint32_t width;
    uint32_t height;
    graphic_platform_framebuffer_size_get(&width, &height);
    if (width == 0 || height == 0)
        return false; /* minimised, retry later */

    if (capabilities.currentExtent.width != UINT32_MAX) {
        backend.swapchain_extent = capabilities.currentExtent;
    } else {
        backend.swapchain_extent.width  = width;
        backend.swapchain_extent.height = height;
    }

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 &&
        image_count > capabilities.maxImageCount)
        image_count = capabilities.maxImageCount;

    if (image_count > GRAPHIC_MAX_SWAPCHAIN_IMAGES)
        image_count = GRAPHIC_MAX_SWAPCHAIN_IMAGES;

    VkSurfaceFormatKHR format = surface_format_pick();

    VkSwapchainCreateInfoKHR info = { 0 };
    info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface          = backend.surface;
    info.minImageCount    = image_count;
    info.imageFormat      = format.format;
    info.imageColorSpace  = format.colorSpace;
    info.imageExtent      = backend.swapchain_extent;
    info.imageArrayLayers = 1;
    info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.preTransform     = capabilities.currentTransform;
    info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode      = present_mode_pick();
    info.clipped          = VK_TRUE;

    uint32_t families[2] = { backend.graphics_family,
                             backend.present_family };

    if (backend.graphics_family != backend.present_family) {
        info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices   = families;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    if (vkCreateSwapchainKHR(backend.device,
                             &info,
                             NULL,
                             &backend.swapchain) != VK_SUCCESS)
        return false;

    backend.swapchain_format      = format.format;
    backend.swapchain_image_count = GRAPHIC_MAX_SWAPCHAIN_IMAGES;
    vkGetSwapchainImagesKHR(backend.device,
                            backend.swapchain,
                            &backend.swapchain_image_count,
                            backend.swapchain_images);

    VkSemaphoreCreateInfo semaphore = { 0 };
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (uint32_t i = 0; i < backend.swapchain_image_count; i++) {
        VkImageViewCreateInfo view = { 0 };
        view.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image                       = backend.swapchain_images[i];
        view.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
        view.format                      = backend.swapchain_format;
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.layerCount = 1;

        if (vkCreateImageView(backend.device,
                              &view,
                              NULL,
                              &backend.swapchain_image_views[i]) != VK_SUCCESS)
            return false;

        if (vkCreateSemaphore(backend.device,
                              &semaphore,
                              NULL,
                              &backend.render_finished[i]) != VK_SUCCESS)
            return false;
    }

    fprintf(stderr, "[graphic] swapchain: %ux%u, %u images, format %d, "
                    "present mode %d\n",
                    backend.swapchain_extent.width,
                    backend.swapchain_extent.height,
                    backend.swapchain_image_count,
                    (int)format.format,
                    (int)info.presentMode);

    return true;
}

static void swapchain_destroy(void)
{
    for (uint32_t i = 0; i < backend.swapchain_image_count; i++) {
        if (backend.swapchain_image_views[i] != VK_NULL_HANDLE)
            vkDestroyImageView(backend.device,
                               backend.swapchain_image_views[i],
                               NULL);

        if (backend.render_finished[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(backend.device,
                               backend.render_finished[i],
                               NULL);

        backend.swapchain_image_views[i] = VK_NULL_HANDLE;
        backend.render_finished[i]       = VK_NULL_HANDLE;
    }

    if (backend.swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(backend.device, backend.swapchain, NULL);

    backend.swapchain             = VK_NULL_HANDLE;
    backend.swapchain_image_count = 0;
}

bool graphic_backend_swapchain_recreate(void)
{
    vkDeviceWaitIdle(backend.device);
    swapchain_destroy();

    return swapchain_create();
}

/* ------------------------------------------------------------------ */
/* per frame resources                                                 */
/* ------------------------------------------------------------------ */

static bool frames_create(void)
{
    VkCommandPoolCreateInfo pool = { 0 };
    pool.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = backend.graphics_family;

    if (vkCreateCommandPool(backend.device,
                            &pool,
                            NULL,
                            &backend.command_pool) != VK_SUCCESS)
        return false;

    VkSemaphoreCreateInfo semaphore = { 0 };
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence = { 0 };
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT; /* no deadlock first frame */

    for (uint32_t i = 0; i < GRAPHIC_FRAMES_IN_FLIGHT; i++) {
        struct graphic_frame *frame = &backend.frames[i];

        VkCommandBufferAllocateInfo allocate = { 0 };
        allocate.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = backend.command_pool;
        allocate.level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(backend.device,
                                     &allocate,
                                     &frame->command_buffer) != VK_SUCCESS)
            return false;

        if (vkCreateSemaphore(backend.device,
                              &semaphore,
                              NULL,
                              &frame->image_available) != VK_SUCCESS)
            return false;

        if (vkCreateFence(backend.device,
                          &fence,
                          NULL,
                          &frame->in_flight) != VK_SUCCESS)
            return false;
    }

    return true;
}

/* ------------------------------------------------------------------ */
/* initialisation                                                     */
/* ------------------------------------------------------------------ */

 bool graphic_backend_init(int         width,
                           int         height,
                           const char *title,
                           uint32_t    flags)
 {
    backend.vsync      = (flags & GRAPHIC_WINDOW_VSYNC)      != 0;
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

    if (!swapchain_create())
        return false;

    if (!graphic_pipeline_init(backend.device, backend.swapchain_format))
        return false;

    if (!frames_create())
        return false;

    return true;
}

/* ------------------------------------------------------------------ */
/* barriers                                                            */
/* ------------------------------------------------------------------ */

/*
 * synchronization2 barrier. Both stage and access masks are 64 bit
 * here, and NONE is a real value rather than the TOP_OF_PIPE hack the
 * original API forced on us.
 */
static void image_barrier(VkCommandBuffer       command_buffer,
                          VkImage               image,
                          VkImageAspectFlags    aspect,
                          VkImageLayout         old_layout,
                          VkImageLayout         new_layout,
                          VkPipelineStageFlags2 source_stage,
                          VkAccessFlags2        source_access,
                          VkPipelineStageFlags2 destination_stage,
                          VkAccessFlags2        destination_access)
{
    VkImageMemoryBarrier2 barrier = { 0 };
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask        = source_stage;
    barrier.srcAccessMask       = source_access;
    barrier.dstStageMask        = destination_stage;
    barrier.dstAccessMask       = destination_access;
    barrier.oldLayout           = old_layout;
    barrier.newLayout           = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;

    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo dependency = { 0 };
    dependency.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers    = &barrier;

    vkCmdPipelineBarrier2(command_buffer, &dependency);
}

/* ------------------------------------------------------------------ */
/* frame begin                                                         */
/* ------------------------------------------------------------------ */

void graphic_backend_clear_color_set(graphic_color_t color)
{
    backend.clear_color.float32[0] = (float)color.r / 255.0f;
    backend.clear_color.float32[1] = (float)color.g / 255.0f;
    backend.clear_color.float32[2] = (float)color.b / 255.0f;
    backend.clear_color.float32[3] = (float)color.a / 255.0f;
}

bool graphic_backend_frame_begin(void)
{
    struct graphic_frame *frame = &backend.frames[backend.frame_index];
    
    if (backend.swapchain_dirty) {
        if (!graphic_backend_swapchain_recreate())
            return false;
        backend.swapchain_dirty = false;
    }


    vkWaitForFences(backend.device,
                    1,
                    &frame->in_flight,
                    VK_TRUE,
                    UINT64_MAX);

    VkResult result;
    result = vkAcquireNextImageKHR(backend.device,
                                   backend.swapchain,
                                   UINT64_MAX,
                                   frame->image_available,
                                   VK_NULL_HANDLE,
                                   &backend.image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        backend.swapchain_dirty = true;
        return false;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        return false;

    /* reset only now: an early return above must leave it signalled */
    vkResetFences(backend.device, 1, &frame->in_flight);

    VkCommandBufferBeginInfo begin = { 0 };
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkResetCommandBuffer(frame->command_buffer, 0);
    vkBeginCommandBuffer(frame->command_buffer, &begin);

    backend.frame_active    = true;
    backend.swapchain_ready = false;

    return true;
}

/* ------------------------------------------------------------------ */
/* recording                                                          */
/* ------------------------------------------------------------------ */

/*
 * Opens the rendering block the frame draws into. The window is target
 * 0, and so far the only target there is.
 */
static void rendering_begin(VkCommandBuffer   command_buffer,
                            uint32_t          target_id,
                            bool              clear,
                            VkClearColorValue clear_color)
{
    VkExtent2D  extent;
    VkImageView color_view;

    if (target_id == 0) {
        extent = backend.swapchain_extent;
        color_view = backend.swapchain_image_views[backend.image_index];

        /*
         * UNDEFINED as the old layout says the previous contents
         * are not needed, which lets a tiled GPU skip loading the
         * tile. Only correct because the first block always
         * clears.
         */
        if (!backend.swapchain_ready) {
            /*
             * The source stage must be the one the acquire
             * semaphore is waited on at. With NONE the transition
             * is not ordered after that wait, and may write the
             * image while the presentation engine still reads
             * it; only synchronization validation notices.
             */
            image_barrier(command_buffer,
                          backend.swapchain_images[backend.image_index],
                          VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_2_NONE,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

            backend.swapchain_ready = true;
        }
    } else {
        return; /* only the window can be drawn into so far */
    }

    VkRenderingAttachmentInfo color = { 0 };
    color.sType            = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView        = color_view;
    color.imageLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp           = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                     VK_ATTACHMENT_LOAD_OP_LOAD;
    color.clearValue.color = clear_color;
    color.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo rendering = { 0 };
    rendering.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent    = extent;
    rendering.layerCount           = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments    = &color;

    vkCmdBeginRendering(command_buffer, &rendering);

    /*
     * Negative height flips Vulkan's Y down NDC back to Y up, which
     * keeps graphic_math.c free of any flip and makes counter
     * clockwise winding the front face. Render targets use the same
     * convention, so their contents are not stored upside down.
     */
    VkViewport viewport;
    viewport.x        = 0.0f;
    viewport.y        = (float)extent.height;
    viewport.width    = (float)extent.width;
    viewport.height   = -(float)extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent   = extent;

    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

static void rendering_end(VkCommandBuffer command_buffer,
                          uint32_t        target_id)
{
    vkCmdEndRendering(command_buffer);
}

/*
 * TODO: temp
 */
static void triangle_draw(VkCommandBuffer       command_buffer,
                          struct graphic_frame *frame)
{
    vkCmdBindPipeline(command_buffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      graphic_pipeline_get());

    vkCmdDraw(command_buffer, 3, 1, 0, 0);
}

/* ------------------------------------------------------------------ */
/* submit and present                                                  */
/* ------------------------------------------------------------------ */

void graphic_backend_frame_end(void)
{
    struct graphic_frame *frame          = &backend.frames[backend.frame_index];
    VkCommandBuffer       command_buffer = frame->command_buffer;

    if (!backend.frame_active)
        return;

    rendering_begin(command_buffer, 0, true, backend.clear_color);
    triangle_draw(command_buffer, frame); // TODO: temp. for test
    rendering_end(command_buffer, 0);

    image_barrier(command_buffer,
                  backend.swapchain_images[backend.image_index],
                  VK_IMAGE_ASPECT_COLOR_BIT,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_NONE,
                  VK_ACCESS_2_NONE);

    vkEndCommandBuffer(command_buffer);

    VkSemaphoreSubmitInfo wait = { 0 };
    wait.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    wait.semaphore = frame->image_available;
    wait.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSemaphoreSubmitInfo signal = { 0 };
    signal.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal.semaphore = backend.render_finished[backend.image_index];
    signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkCommandBufferSubmitInfo command = { 0 };
    command.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command.commandBuffer = command_buffer;

    VkSubmitInfo2 submit = { 0 };
    submit.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.waitSemaphoreInfoCount   = 1;
    submit.pWaitSemaphoreInfos      = &wait;
    submit.commandBufferInfoCount   = 1;
    submit.pCommandBufferInfos      = &command;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos    = &signal;

    vkQueueSubmit2(backend.graphics_queue, 1, &submit, frame->in_flight);

    VkPresentInfoKHR present = { 0 };
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &backend.render_finished[backend.image_index];
    present.swapchainCount     = 1;
    present.pSwapchains        = &backend.swapchain;
    present.pImageIndices      = &backend.image_index;

    VkResult result;
    result = vkQueuePresentKHR(backend.present_queue, &present);

    if (result == VK_ERROR_OUT_OF_DATE_KHR ||
        result == VK_SUBOPTIMAL_KHR)
        backend.swapchain_dirty = true;

    backend.frame_active = false;
    backend.frame_index  = (backend.frame_index + 1) % GRAPHIC_FRAMES_IN_FLIGHT;
}

/* ------------------------------------------------------------------ */
/* shutdown                                                           */
/* ------------------------------------------------------------------ */

void graphic_backend_shutdown(void)
{
    if (backend.device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(backend.device);

    graphic_pipeline_shutdown();

    for (uint32_t i = 0; i < GRAPHIC_FRAMES_IN_FLIGHT; i++) {
        struct graphic_frame *frame = &backend.frames[i];

        if (frame->image_available != VK_NULL_HANDLE)
            vkDestroySemaphore(backend.device,
                               frame->image_available,
                               NULL);

        if (frame->in_flight != VK_NULL_HANDLE)
            vkDestroyFence(backend.device,
                           frame->in_flight,
                           NULL);
    }

    if (backend.command_pool != VK_NULL_HANDLE)
        vkDestroyCommandPool(backend.device,
                             backend.command_pool,
                             NULL);

    swapchain_destroy();

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
