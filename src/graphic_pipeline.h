#ifndef GRAPHIC_PIPELINE_H
#define GRAPHIC_PIPELINE_H

#include <stdbool.h>

#include <vulkan/vulkan.h>

/*
 * Sixty four bytes, inside the hundred and twenty eight every
 * implementation guarantees: the view projection matrix and nothing
 * else, which is all the vertex stage needs.
 */
struct graphic_push_constants {
    float view_projection[16];
};

bool graphic_pipeline_init(VkDevice device,
                           VkFormat color_format);

void graphic_pipeline_shutdown(void);

/*
 * mode is enum graphic_draw_mode. Quads share the triangle pipeline.
 */
VkPipeline graphic_pipeline_get(int mode);

VkPipelineLayout graphic_pipeline_layout_get(void);

#endif /* GRAPHIC_PIPELINE_H */