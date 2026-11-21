#include "renderer/graphics_pipeline.h"

#include "vk/shader.h"

namespace rtrt
{

namespace
{

// The fixed-function state both graphics pipelines share.
struct FixedFunctionState
{
    VkPipelineInputAssemblyStateCreateInfo InputAssembly{};
    VkViewport Viewport{};
    VkRect2D Scissor{};
    VkPipelineViewportStateCreateInfo ViewportState{};
    VkPipelineRasterizationStateCreateInfo Rasterizer{};
    VkPipelineMultisampleStateCreateInfo Multisampling{};
    VkPipelineDepthStencilStateCreateInfo DepthStencil{};
    VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
    VkPipelineColorBlendStateCreateInfo ColorBlending{};
};

FixedFunctionState MakeFixedFunctionState(VkExtent2D Size)
{
    FixedFunctionState State;

    State.InputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    State.InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    State.InputAssembly.primitiveRestartEnable = VK_FALSE;

    State.Viewport.x = 0.0f;
    State.Viewport.y = 0.0f;
    State.Viewport.width = static_cast<float>(Size.width);
    State.Viewport.height = static_cast<float>(Size.height);
    State.Viewport.minDepth = 0.0f;
    State.Viewport.maxDepth = 1.0f;

    State.Scissor.offset = {0, 0};
    State.Scissor.extent = Size;

    State.Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    State.Rasterizer.depthClampEnable = VK_FALSE;
    State.Rasterizer.rasterizerDiscardEnable = VK_FALSE;
    State.Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    State.Rasterizer.lineWidth = 1.0f;
    State.Rasterizer.cullMode = VK_CULL_MODE_NONE;
    State.Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    State.Rasterizer.depthBiasEnable = VK_FALSE;

    State.Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    State.Multisampling.sampleShadingEnable = VK_FALSE;
    State.Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    State.DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    State.DepthStencil.depthTestEnable = VK_TRUE;
    State.DepthStencil.depthWriteEnable = VK_TRUE;
    // BEWARE!!  NECESSARY!!
    State.DepthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    State.DepthStencil.depthBoundsTestEnable = VK_FALSE;
    State.DepthStencil.stencilTestEnable = VK_FALSE;

    State.ColorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    State.ColorBlendAttachment.blendEnable = VK_FALSE;

    State.ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    State.ColorBlending.logicOpEnable = VK_FALSE;
    State.ColorBlending.logicOp = VK_LOGIC_OP_COPY;
    State.ColorBlending.attachmentCount = 1;
    State.ColorBlending.blendConstants[0] = 0.0f;
    State.ColorBlending.blendConstants[1] = 0.0f;
    State.ColorBlending.blendConstants[2] = 0.0f;
    State.ColorBlending.blendConstants[3] = 0.0f;
    return State;
}

}  // namespace

Pipeline CreateFullWindowPipeline(VkDevice LogicalDevice, bool UseSlang,
                                  const GraphicsShaders& Shaders,
                                  const VkPipelineVertexInputStateCreateInfo& VertexInput,
                                  VkPipelineLayout Layout, VkRenderPass Pass,
                                  VkExtent2D Size)
{
    // The pipeline keeps compiled copies, so the modules are freed when this returns.
    Shader Vert;
    Vert.Load(LogicalDevice, Shaders.VertGlsl, Shaders.VertSlang, UseSlang);
    Shader Frag;
    Frag.Load(LogicalDevice, Shaders.FragGlsl, Shaders.FragSlang, UseSlang);

    VkPipelineShaderStageCreateInfo ShaderStages[] = {
        Vert.StageInfo(VK_SHADER_STAGE_VERTEX_BIT),
        Frag.StageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    // The state points into itself, so it's wired up here rather than copied around.
    FixedFunctionState State = MakeFixedFunctionState(Size);
    State.ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    State.ViewportState.viewportCount = 1;
    State.ViewportState.pViewports = &State.Viewport;
    State.ViewportState.scissorCount = 1;
    State.ViewportState.pScissors = &State.Scissor;
    State.ColorBlending.pAttachments = &State.ColorBlendAttachment;

    VkGraphicsPipelineCreateInfo PipelineInfo{};
    PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineInfo.stageCount = 2;
    PipelineInfo.pStages = ShaderStages;
    PipelineInfo.pVertexInputState = &VertexInput;
    PipelineInfo.pInputAssemblyState = &State.InputAssembly;
    PipelineInfo.pViewportState = &State.ViewportState;
    PipelineInfo.pRasterizationState = &State.Rasterizer;
    PipelineInfo.pMultisampleState = &State.Multisampling;
    PipelineInfo.pDepthStencilState = &State.DepthStencil;
    PipelineInfo.pColorBlendState = &State.ColorBlending;
    PipelineInfo.layout = Layout;
    PipelineInfo.renderPass = Pass;
    PipelineInfo.subpass = 0;
    PipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    Pipeline Result;
    Result.CreateGraphics(LogicalDevice, PipelineInfo);
    return Result;
}

}  // namespace rtrt
