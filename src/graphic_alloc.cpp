// The single C++ translation unit of the project. It exists only
// because VMA ships as a C++ header; the interface it exposes in
// graphic_alloc.h is plain C.

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 1
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

#ifdef __cplusplus
}
#endif