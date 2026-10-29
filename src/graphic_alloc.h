#ifndef GRAPHIC_ALLOC_H
#define GRAPHIC_ALLOC_H

#include <stdbool.h>

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Thin C facade over the Vulkan Memory Allocator. graphic_alloc.cpp is the
 * only C++ translation unit in the project; replacing it with a hand
 * written suballocator would not change a single caller.
 */

struct graphic_buffer {
    VkBuffer     handle;
    void        *mapped;        /* NULL unless host visible */
    VkDeviceSize size;
    void        *allocation;    /* VmaAllocation */
};

/*
 * Named gpu_image, not image: graphic_image_t in the public header is
 * a block of CPU pixels for window icons, a different thing entirely.
 */
struct graphic_gpu_image {
    VkImage     handle;
    VkImageView view;
    void       *allocation;    /* VmaAllocation */
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

bool graphic_gpu_image_create(struct graphic_gpu_image *image,
                              uint32_t                  width,
                              uint32_t                  height,
                              VkFormat                  format,
                              VkImageUsageFlags         usage,
                              VkImageAspectFlags        aspect,
                              VkSampleCountFlagBits     samples);

void graphic_gpu_image_destroy(struct graphic_gpu_image *image);

#ifdef __cplusplus
}
#endif

#endif /* GRAPHIC_ALLOC_H */