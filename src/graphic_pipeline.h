#ifndef GRAPHIC_PIPELINE_H
#define GRAPHIC_PIPELINE_H

#include <stdbool.h>

#include <vulkan/vulkan.h>

bool graphic_pipeline_init(VkDevice device,
                           VkFormat color_format);

void graphic_pipeline_shutdown(void);

VkPipeline graphic_pipeline_get(void);

#endif /* GRAPHIC_PIPELINE_H */