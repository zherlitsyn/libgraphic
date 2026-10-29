// The single C++ translation unit of the project. It exists only
// because VMA ships as a C++ header; the interface it exposes in
// graphic_alloc.h is plain C.

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS  1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>

#include "graphic_alloc.h"

namespace {
    VmaAllocator allocator        = VK_NULL_HANDLE;
    VkDevice     allocator_device = VK_NULL_HANDLE;
}  // namespace

#ifdef __cplusplus
extern "C" {
#endif

bool graphic_alloc_init(VkInstance       instance,
                        VkPhysicalDevice physical_device,
                        VkDevice         device,
                        uint32_t         api_version)
{
    VmaAllocatorCreateInfo info = {};
    info.instance         = instance;
    info.physicalDevice   = physical_device;
    info.device           = device;
    info.vulkanApiVersion = api_version;

    if (vmaCreateAllocator(&info, &allocator) != VK_SUCCESS)
        return false;

    allocator_device = device;

    return true;
}

void graphic_alloc_shutdown(void)
{
    if (allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator);
        allocator = VK_NULL_HANDLE;
    }

    allocator_device = VK_NULL_HANDLE;
}

bool graphic_buffer_create(struct graphic_buffer *buffer,
                           VkDeviceSize           size,
                           VkBufferUsageFlags     usage,
                           bool                   host_visible)
{
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size        = size;
    buffer_info.usage       = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocation_info = {};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO;

    if (host_visible) {
        // Sequential write plus persistent mapping: the batch
        // writes straight into this memory and never reads back.
        allocation_info.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VkBuffer          handle      = VK_NULL_HANDLE;
    VmaAllocation     allocation  = VK_NULL_HANDLE;
    VmaAllocationInfo result_info = {};
    if (vmaCreateBuffer(allocator,
                        &buffer_info,
                        &allocation_info,
                        &handle,
                        &allocation,
                        &result_info) != VK_SUCCESS)
        return false;

    buffer->handle     = handle;
    buffer->allocation = allocation;
    buffer->mapped     = host_visible ? result_info.pMappedData : nullptr;
    buffer->size       = size;

    return true;
}

void graphic_buffer_destroy(struct graphic_buffer *buffer)
{
    if (buffer->handle == VK_NULL_HANDLE)
        return;

    vmaDestroyBuffer(allocator,
                     buffer->handle,
                     static_cast<VmaAllocation>(buffer->allocation));

    buffer->handle     = VK_NULL_HANDLE;
    buffer->allocation = nullptr;
    buffer->mapped     = nullptr;
    buffer->size       = 0;
}

bool graphic_gpu_image_create(struct graphic_gpu_image *image,
                              uint32_t                  width,
                              uint32_t                  height,
                              VkFormat                  format,
                              VkImageUsageFlags         usage,
                              VkImageAspectFlags        aspect,
                              VkSampleCountFlagBits     samples)
{
    VkImageCreateInfo image_info = {};
    image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType     = VK_IMAGE_TYPE_2D;
    image_info.format        = format;
    image_info.extent.width  = width;
    image_info.extent.height = height;
    image_info.extent.depth  = 1;
    image_info.mipLevels     = 1;
    image_info.arrayLayers   = 1;
    image_info.samples       = samples;
    image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage         = usage;
    image_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocation_info = { 0 };
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO;

    // A depth buffer is written and discarded every frame, so a
    // tiled GPU can keep it on chip and never write it out. That is
    // what lazily allocated memory is for, and it is only legal for
    // an image created with VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
    // which the depth images now are. Where no such heap exists the
    // preference is simply ignored.
    if (usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
        allocation_info.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;

    VkImage       handle     = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    if (vmaCreateImage(allocator,
                       &image_info,
                       &allocation_info,
                       &handle,
                       &allocation,
                       nullptr) != VK_SUCCESS)
        return false;

    VkImageViewCreateInfo view_info = {};
    view_info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image    = handle;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format   = format;

    view_info.subresourceRange.aspectMask = aspect;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(allocator_device,
                          &view_info,
                          nullptr,
                          &view) != VK_SUCCESS) {
        vmaDestroyImage(allocator, handle, allocation);
        return false;
    }

    image->handle     = handle;
    image->view       = view;
    image->allocation = allocation;

    return true;
}

void graphic_gpu_image_destroy(struct graphic_gpu_image *image)
{
    if (image->handle == VK_NULL_HANDLE)
        return;

    if (image->view != VK_NULL_HANDLE)
        vkDestroyImageView(allocator_device, image->view, nullptr);

    vmaDestroyImage(allocator,
                    image->handle,
                    static_cast<VmaAllocation>(image->allocation));

    image->handle     = VK_NULL_HANDLE;
    image->view       = VK_NULL_HANDLE;
    image->allocation = nullptr;
}

#ifdef __cplusplus
}
#endif