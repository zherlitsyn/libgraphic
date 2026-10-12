#ifndef GRAPHIC_ALLOC_H
#define GRAPHIC_ALLOC_H

#include <stdbool.h>

#include <vulkan/vulkan.h>

/*
 * The memory layer's interface. For now a hand written allocator sits
 * behind it, one vkAllocateMemory per buffer; the next lecture puts the
 * Vulkan Memory Allocator there instead without changing a caller.
 */

struct graphic_buffer {
    VkBuffer     handle;
    void        *mapped;        /* NULL unless host visible */
    VkDeviceSize size;
    void        *allocation;    /* VkDeviceMemory, for now */
};

bool graphic_alloc_init(VkInstance       instance,
                        VkPhysicalDevice physical_device,
                        VkDevice         device,
                        uint32_t         api_version);

void graphic_alloc_shutdown(void);

bool graphic_buffer_create(struct graphic_buffer *buffer,
                           VkDeviceSize           size,
                           VkBufferUsageFlags     usage,
                           bool                   host_visible);

void graphic_buffer_destroy(struct graphic_buffer *buffer);

#endif /* GRAPHIC_ALLOC_H */