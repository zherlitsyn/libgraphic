#include <stddef.h>

#include "graphic_alloc.h"

static struct {
    VkDevice                         device;
    VkPhysicalDeviceMemoryProperties properties;
} memory;

bool graphic_alloc_init(VkInstance       instance,
                        VkPhysicalDevice physical_device,
                        VkDevice         device,
                        uint32_t         api_version)
{
    (void)instance;
    (void)api_version;

    memory.device = device;

    /* the heaps, and the memory types carved out of them */
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory.properties);

    return true;
}

void graphic_alloc_shutdown(void)
{
    memory.device = VK_NULL_HANDLE;
}

/*
 * The first memory type that can back the resource and has every
 * property asked for. type_bits comes from the resource itself: bit i
 * set means memory type i is acceptable to it.
 */
static int memory_type_find(uint32_t type_bits, VkMemoryPropertyFlags wanted)
{
    for (uint32_t i = 0; i < memory.properties.memoryTypeCount; i++) {
        VkMemoryPropertyFlags flags =
            memory.properties.memoryTypes[i].propertyFlags;

        if ((type_bits & (1u << i)) && (flags & wanted) == wanted)
            return (int)i;
    }

    return -1;
}

bool graphic_buffer_create(struct graphic_buffer *buffer,
                           VkDeviceSize           size,
                           VkBufferUsageFlags     usage,
                           bool                   host_visible)
{
    VkBufferCreateInfo info = { 0 };
    info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size        = size;
    info.usage       = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer handle = VK_NULL_HANDLE;
    if (vkCreateBuffer(memory.device, &info, NULL, &handle) != VK_SUCCESS)
        return false;

    /*
     * Coherent memory needs no vkFlushMappedMemoryRanges: what the CPU
     * has written by the time of the submit is what the GPU reads.
     */
    VkMemoryPropertyFlags wanted;
    wanted = host_visible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT :
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(memory.device, handle, &requirements);

    int type = memory_type_find(requirements.memoryTypeBits, wanted);
    if (type < 0)
        goto fail;

    VkMemoryAllocateInfo allocate = { 0 };
    allocate.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate.allocationSize  = requirements.size;
    allocate.memoryTypeIndex = (uint32_t)type;

    VkDeviceMemory device_memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(memory.device,
                         &allocate,
                         NULL,
                         &device_memory) != VK_SUCCESS)
        goto fail;

    if (vkBindBufferMemory(memory.device, handle, device_memory, 0) !=
        VK_SUCCESS)
        goto fail;

    /* mapped once, for the whole life of the buffer */
    void *mapped = NULL;
    if (host_visible && vkMapMemory(memory.device,
                                    device_memory,
                                    0,
                                    VK_WHOLE_SIZE,
                                    0,
                                    &mapped) != VK_SUCCESS)
        goto fail;

    buffer->handle = handle;
    buffer->mapped = mapped;
    buffer->size = size;
    buffer->allocation = (void *)device_memory;

    return true;

fail:
    if (device_memory != VK_NULL_HANDLE)
        vkFreeMemory(memory.device, device_memory, NULL);

    vkDestroyBuffer(memory.device, handle, NULL);

    return false;
}

void graphic_buffer_destroy(struct graphic_buffer *buffer)
{
    if (buffer->handle == VK_NULL_HANDLE)
        return;

    vkDestroyBuffer(memory.device, buffer->handle, NULL);

    /* freeing memory that is still mapped unmaps it as well */
    vkFreeMemory(memory.device, (VkDeviceMemory)buffer->allocation, NULL);

    buffer->handle     = VK_NULL_HANDLE;
    buffer->allocation = NULL;
    buffer->mapped     = NULL;
    buffer->size       = 0;
}