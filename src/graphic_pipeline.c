#include <stddef.h>
#include <stdio.h>
#include <vulkan/vulkan.h>

#include "graphic_internal.h"
#include "graphic_pipeline.h"

#include "default.vert.h"
#include "default.frag.h"

/*
 * Two pipelines only. Everything that can be a dynamic state is one,
 * so the pair differs solely in primitive topology.
 *
 * Topology could in principle be dynamic too (vkCmdSetPrimitiveTopology
 * is core in 1.3), but switching between topology classes such as line
 * and triangle additionally requires the dynamicPrimitiveTopology-
 * Unrestricted property, which is not guaranteed. Two pipelines are
 * free at runtime and portable.
 */
enum graphic_pipeline_slot {
    GRAPHIC_PIPELINE_SLOT_TRIANGLES = 0,
    GRAPHIC_PIPELINE_SLOT_LINES,
    GRAPHIC_PIPELINE_SLOT_COUNT,
};

struct graphic_pipeline_state {
    VkDevice         device;
    VkPipelineLayout layout;

    /*
     * Blending is pipeline state in Vulkan 1.3 core, so the table is
     * two dimensional. Entries are filled in on demand: a program
     * that only ever uses alpha blending compiles two pipelines, not
     * twelve.
     */
    VkPipeline pipelines[GRAPHIC_PIPELINE_SLOT_COUNT][GRAPHIC_BLEND_MODE_COUNT];

    /* kept alive for lazy compilation, unlike the usual practice */
    VkShaderModule   vertex_module;
    VkShaderModule   fragment_module;
    VkFormat         color_format;
    VkFormat         depth_format;
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
    VkPushConstantRange range = { 0 };
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    range.offset     = 0;
    range.size       = sizeof(struct graphic_push_constants);

    VkPipelineLayoutCreateInfo info = { 0 };
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges    = &range;

    return vkCreatePipelineLayout(pipeline_state.device,
                                  &info,
                                  NULL,
                                  &pipeline_state.layout) == VK_SUCCESS;
}

/*
 * Factors chosen to match what raylib produces on OpenGL, so material
 * settings port over unchanged.
 */
static void blend_attachment_fill(int                                  blend_mode,
                                  VkPipelineColorBlendAttachmentState *out)
{
    out->blendEnable         = VK_TRUE;
    out->colorBlendOp        = VK_BLEND_OP_ADD;
    out->alphaBlendOp        = VK_BLEND_OP_ADD;
    out->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    out->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    out->colorWriteMask      = VK_COLOR_COMPONENT_R_BIT |
                               VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT |
                               VK_COLOR_COMPONENT_A_BIT;

    switch (blend_mode) {
        case GRAPHIC_BLEND_ADDITIVE:
            out->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            out->dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            break;
        case GRAPHIC_BLEND_MULTIPLIED:
            out->srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
            out->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;
        case GRAPHIC_BLEND_ADD_COLORS:
            out->srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            out->dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            break;
        case GRAPHIC_BLEND_SUBTRACT_COLORS:
            out->srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            out->dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            out->colorBlendOp        = VK_BLEND_OP_SUBTRACT;
            out->alphaBlendOp        = VK_BLEND_OP_SUBTRACT;
            break;
        case GRAPHIC_BLEND_ALPHA_PREMULTIPLY:
            out->srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            out->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;
        case GRAPHIC_BLEND_ALPHA:
        default:
            out->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            out->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;
    }
}

static bool pipeline_create_full(VkPrimitiveTopology topology,
                                 int                 blend_mode,
                                 VkShaderModule      vertex_module,
                                 VkShaderModule      fragment_module,
                                 VkPipelineLayout    pipeline_layout,
                                 VkFormat            color_format,
                                 VkFormat            depth_format,
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

    VkVertexInputBindingDescription binding[1] = { { 0 } };
    binding[0].binding   = 0;
    binding[0].stride    = sizeof(struct graphic_vertex);
    binding[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[4] = { { 0 } };
    attributes[0].location = 0;
    attributes[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset   = offsetof(struct graphic_vertex, position);

    attributes[1].location = 1;
    attributes[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset   = offsetof(struct graphic_vertex, normal);

    /* UNORM: the eight bit channels arrive in the shader as [0, 1] */
    attributes[2].location = 2;
    attributes[2].format   = VK_FORMAT_R8G8B8A8_UNORM;
    attributes[2].offset   = offsetof(struct graphic_vertex, color);

    attributes[3].location = 3;
    attributes[3].format   = VK_FORMAT_R32G32_SFLOAT;
    attributes[3].offset   = offsetof(struct graphic_vertex, texcoord);

    VkPipelineVertexInputStateCreateInfo vertex_input = { 0 };
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount   = 1;
    vertex_input.pVertexBindingDescriptions      = binding;
    vertex_input.vertexAttributeDescriptionCount = 4;
    vertex_input.pVertexAttributeDescriptions    = attributes;

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
    /* cullMode and frontFace are dynamic, set per frame */

    VkPipelineMultisampleStateCreateInfo multisample = { 0 };
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil = { 0 };
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    /* depthTestEnable and depthWriteEnable are dynamic */

    VkPipelineColorBlendAttachmentState blend_attachment = { 0 };
    blend_attachment_fill(blend_mode, &blend_attachment);

    VkPipelineColorBlendStateCreateInfo blend = { 0 };
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments    = &blend_attachment;

    const VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_CULL_MODE,
        VK_DYNAMIC_STATE_FRONT_FACE,
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
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
    rendering.depthAttachmentFormat   = depth_format;

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
    info.pDepthStencilState  = &depth_stencil;
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
                            int                 blend_mode,
                            VkShaderModule      vertex_module,
                            VkShaderModule      fragment_module,
                            VkFormat            color_format,
                            VkFormat            depth_format,
                            VkPipeline         *out)
{
    return pipeline_create_full(topology,
                                blend_mode,
                                vertex_module,
                                fragment_module,
                                pipeline_state.layout,
                                color_format,
                                depth_format,
                                out);
}

bool graphic_pipeline_blend_mode_prepare(int blend_mode)
{
    static const VkPrimitiveTopology topology[] = {
        [GRAPHIC_PIPELINE_SLOT_TRIANGLES] = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        [GRAPHIC_PIPELINE_SLOT_LINES]     = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    };

    if (blend_mode < 0 || blend_mode >= GRAPHIC_BLEND_MODE_COUNT)
        return false;

    if (pipeline_state.device == VK_NULL_HANDLE)
        return false;

    for (int slot = 0; slot < GRAPHIC_PIPELINE_SLOT_COUNT; slot++) {
        if (pipeline_state.pipelines[slot][blend_mode] != VK_NULL_HANDLE)
            continue;

        if (!pipeline_create(topology[slot],
                             blend_mode,
                             pipeline_state.vertex_module,
                             pipeline_state.fragment_module,
                             pipeline_state.color_format,
                             pipeline_state.depth_format,
                             &pipeline_state.pipelines[slot][blend_mode])) {
            fprintf(stderr, "[graphic] pipeline creation failed for blend mode %d\n",
                            blend_mode);
            return false;
        }
    }

    return true;
}

bool graphic_pipeline_init(VkDevice device,
                           VkFormat color_format,
                           VkFormat depth_format)
{
    pipeline_state.device       = device;
    pipeline_state.color_format = color_format;
    pipeline_state.depth_format = depth_format;

    if (!layout_create()) {
        fprintf(stderr, "[graphic] pipeline layout creation failed\n");
        return false;
    }

    /*
     * The modules outlive initialisation here, unlike the usual
     * advice to destroy them straight away, because pipelines for
     * the other blend modes are compiled later on demand.
     */
    pipeline_state.vertex_module = 
        shader_module_create(default_vert_spv,
                             sizeof(default_vert_spv));

    pipeline_state.fragment_module =
        shader_module_create(default_frag_spv,
                             sizeof(default_frag_spv));

    if (pipeline_state.vertex_module   == VK_NULL_HANDLE ||
        pipeline_state.fragment_module == VK_NULL_HANDLE)
        return false;

    /* the default mode is always ready, so drawing never stalls */
    return graphic_pipeline_blend_mode_prepare(GRAPHIC_BLEND_ALPHA);
}

VkPipeline graphic_pipeline_get(int mode,
                                int blend_mode)
{
    int slot = (mode == GRAPHIC_DRAW_LINES) ? 
               GRAPHIC_PIPELINE_SLOT_LINES :
               GRAPHIC_PIPELINE_SLOT_TRIANGLES;

    if (blend_mode < 0 || blend_mode >= GRAPHIC_BLEND_MODE_COUNT)
        blend_mode = GRAPHIC_BLEND_ALPHA;

    return pipeline_state.pipelines[slot][blend_mode];
}

VkPipelineLayout graphic_pipeline_layout_get(void)
{
    return pipeline_state.layout;
}

void graphic_pipeline_shutdown(void)
{
    if (pipeline_state.device == VK_NULL_HANDLE)
        return;

    for (int slot = 0; slot < GRAPHIC_PIPELINE_SLOT_COUNT; slot++) {
        for (int blend = 0; blend < GRAPHIC_BLEND_MODE_COUNT; blend++) {
            if (pipeline_state.pipelines[slot][blend] != VK_NULL_HANDLE)
                vkDestroyPipeline(pipeline_state.device,
                                  pipeline_state.pipelines[slot][blend],
                                  NULL);
        }
    }

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