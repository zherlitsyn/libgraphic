#include <stdio.h>
#include <vulkan/vulkan.h>

#include "graphic_internal.h"
#include "graphic_pipeline.h"

#include "default.vert.h"
#include "default.frag.h"

struct graphic_pipeline_state {
    VkDevice         device;
    VkPipelineLayout layout;

    VkPipeline       pipeline;

    VkShaderModule   vertex_module;
    VkShaderModule   fragment_module;
    VkFormat         color_format;
};

static struct graphic_pipeline_state pipeline_state;

static VkShaderModule shader_module_create(const uint32_t *code,
                                           size_t          size_bytes)
{
    VkShaderModuleCreateInfo info = { 0 };
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size_bytes;
    info.pCode    = code;

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(pipeline_state.device, &info, NULL,
                             &module) != VK_SUCCESS)
        return VK_NULL_HANDLE;

    return module;
}

static bool layout_create(void)
{
    VkPipelineLayoutCreateInfo info = { 0 };
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    return vkCreatePipelineLayout(pipeline_state.device,
                                  &info,
                                  NULL,
                                  &pipeline_state.layout) == VK_SUCCESS;
}

static bool pipeline_create_full(VkPrimitiveTopology topology,
                                 VkShaderModule      vertex_module,
                                 VkShaderModule      fragment_module,
                                 VkPipelineLayout    pipeline_layout,
                                 VkFormat            color_format,
                                 VkPipeline         *out)
{
    VkPipelineShaderStageCreateInfo stages[2] = { { 0 } };
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex_module;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragment_module;
    stages[1].pName  = "main";

    VkPipelineVertexInputStateCreateInfo vertex_input = { 0 };
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = { 0 };
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = topology;

    VkPipelineViewportStateCreateInfo viewport = { 0 };
    /* counts stay static even though both states are dynamic */
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterization = { 0 };
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    /*
     * Anything above 1.0 needs the wideLines feature, which mobile
     * drivers and V3DV commonly lack. Keep it at one.
     */
    rasterization.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = { 0 };
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend_attachment = { 0 };
    /* no blending yet: every fragment replaces what was there */
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                      VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT |
                                      VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend = { 0 };
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments    = &blend_attachment;

    const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic = { 0 };
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = sizeof(dynamic_states) /
                                sizeof(dynamic_states[0]);
    dynamic.pDynamicStates    = dynamic_states;

    /*
     * Dynamic rendering: the pipeline is tied to attachment formats
     * instead of a VkRenderPass. This is what keeps pipelines valid
     * across swapchain recreation.
     */
    VkPipelineRenderingCreateInfo rendering = { 0 };
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount    = 1;
    rendering.pColorAttachmentFormats = &color_format;

    VkGraphicsPipelineCreateInfo info = { 0 };
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext               = &rendering;
    info.stageCount          = 2;
    info.pStages             = stages;
    info.pVertexInputState   = &vertex_input;
    info.pInputAssemblyState = &input_assembly;
    info.pViewportState      = &viewport;
    info.pRasterizationState = &rasterization;
    info.pMultisampleState   = &multisample;
    info.pColorBlendState    = &blend;
    info.pDynamicState       = &dynamic;
    info.layout              = pipeline_layout;
    info.renderPass          = VK_NULL_HANDLE; /* required to be null */

    return vkCreateGraphicsPipelines(pipeline_state.device,
                                     VK_NULL_HANDLE,
                                     1,
                                     &info,
                                     NULL,
                                     out) == VK_SUCCESS;
}

static bool pipeline_create(VkPrimitiveTopology topology,
                            VkShaderModule      vertex_module,
                            VkShaderModule      fragment_module,
                            VkFormat            color_format,
                            VkPipeline         *out)
{
    return pipeline_create_full(topology,
                                vertex_module,
                                fragment_module,
                                pipeline_state.layout,
                                color_format,
                                out);
}

bool graphic_pipeline_init(VkDevice device,
                           VkFormat color_format)
{
    pipeline_state.device       = device;
    pipeline_state.color_format = color_format;

    if (!layout_create()) {
        fprintf(stderr, "[graphic] pipeline layout creation failed\n");
        return false;
    }

    pipeline_state.vertex_module = 
        shader_module_create(default_vert_spv,
                             sizeof(default_vert_spv));

    pipeline_state.fragment_module =
        shader_module_create(default_frag_spv,
                             sizeof(default_frag_spv));

    if (pipeline_state.vertex_module   == VK_NULL_HANDLE ||
        pipeline_state.fragment_module == VK_NULL_HANDLE)
        return false;

    if (!pipeline_create(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                         pipeline_state.vertex_module,
                         pipeline_state.fragment_module,
                         pipeline_state.color_format,
                         &pipeline_state.pipeline)) {
        fprintf(stderr, "[graphic] pipeline creation failed\n");
        return false;
    }

    return true;
}

VkPipeline graphic_pipeline_get(void)
{
    return pipeline_state.pipeline;
}

void graphic_pipeline_shutdown(void)
{
    if (pipeline_state.device == VK_NULL_HANDLE)
        return;

    if (pipeline_state.pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(pipeline_state.device,
                          pipeline_state.pipeline,
                          NULL);

    if (pipeline_state.vertex_module != VK_NULL_HANDLE)
        vkDestroyShaderModule(pipeline_state.device,
                              pipeline_state.vertex_module,
                              NULL);

    if (pipeline_state.fragment_module != VK_NULL_HANDLE)
        vkDestroyShaderModule(pipeline_state.device,
                              pipeline_state.fragment_module,
                              NULL);

    if (pipeline_state.layout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(pipeline_state.device,
                                pipeline_state.layout,
                                NULL);

    pipeline_state.device = VK_NULL_HANDLE;
}