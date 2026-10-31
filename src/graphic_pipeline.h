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
                           VkFormat color_format,
                           VkFormat depth_format);

void graphic_pipeline_shutdown(void);

/*
 * mode is enum graphic_draw_mode, blend_mode is enum
 * graphic_blend_mode. Quads share the triangle pipeline.
 *
 * _prepare compiles the pair for a blend mode; _get only looks up and
 * returns VK_NULL_HANDLE if it was never prepared, so no compilation
 * ever happens inside a command buffer.
 */
bool graphic_pipeline_blend_mode_prepare(int blend_mode);

VkPipeline graphic_pipeline_get(int mode,
                                int blend_mode);

VkPipelineLayout graphic_pipeline_layout_get(void);

#endif /* GRAPHIC_PIPELINE_H */