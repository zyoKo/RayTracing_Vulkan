#include "renderer.h"

#include <array>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include "vk_check.h"

namespace rtrt
{

void Renderer::DestroyAllVulkanResources()
{
    RTRT_CHECK_VK(vkDeviceWaitIdle(GpuDevice->GetHandle()));

    // All objects created on Device must be destroyed before Device.
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(GpuDevice->GetHandle(), ImguiDescPool, nullptr);

    MainSwapchain->Destroy();
    FrameFence.Destroy();
    MainCmdPool.Destroy();

    DestroyPostResources();
    DestroySceneResources();
    DestroyScanlineResources();
    DestroyRaytracingResources();
    DestroyDenoiseResources();
}

void Renderer::DestroyPostResources()
{
    DepthImage.Destroy();
    vkDestroyRenderPass(GpuDevice->GetHandle(), PostRenderPass, nullptr);

    for (auto& Framebuffer : Framebuffers)
    {
        vkDestroyFramebuffer(GpuDevice->GetHandle(), Framebuffer, nullptr);
    }

    ScImageBuffer.Destroy();
    PostDesc.Destroy(GpuDevice->GetHandle());

    vkDestroyPipelineLayout(GpuDevice->GetHandle(), PostPipelineLayout, nullptr);
    vkDestroyPipeline(GpuDevice->GetHandle(), PostPipeline, nullptr);
}

void Renderer::DestroySceneResources()
{
    for (auto& T : ObjTextures)
    {
        T.Destroy();
    }
    // ObjDescList only holds device addresses; the buffers themselves live in
    // ObjDataList.
    for (auto& Ob : ObjDataList)
    {
        Ob.VertexBuffer.Destroy();
        Ob.IndexBuffer.Destroy();
        Ob.MatColorBuffer.Destroy();
        Ob.MatIndexBuffer.Destroy();
    }

    MatrixBw.Destroy();
    ObjDescriptionBw.Destroy();
}

void Renderer::DestroyScanlineResources()
{
    vkDestroyRenderPass(GpuDevice->GetHandle(), ScanlineRenderPass, nullptr);
    vkDestroyFramebuffer(GpuDevice->GetHandle(), ScanlineFramebuffer, nullptr);
    ScDesc.Destroy(GpuDevice->GetHandle());
    vkDestroyPipelineLayout(GpuDevice->GetHandle(), ScanlinePipelineLayout, nullptr);
    vkDestroyPipeline(GpuDevice->GetHandle(), ScanlinePipeline, nullptr);
}

void Renderer::DestroyRaytracingResources()
{
    RtColCurrBuffer.Destroy();
    RtBuilder.Destroy();
    RtDesc.Destroy(GpuDevice->GetHandle());
    vkDestroyPipelineLayout(GpuDevice->GetHandle(), RtPipelineLayout, nullptr);
    vkDestroyPipeline(GpuDevice->GetHandle(), RtPipeline, nullptr);
    ShaderBindingTableBw.Destroy();

    // History buffers
    RtColPrevBuffer.Destroy();
    RtNdCurrBuffer.Destroy();
    RtNdPrevBuffer.Destroy();
    RtKdCurrBuffer.Destroy();
    RtKdPrevBuffer.Destroy();
}

void Renderer::DestroyDenoiseResources()
{
    DenoiseBuffer.Destroy();
    DenoiseDesc.Destroy(GpuDevice->GetHandle());
    vkDestroyPipelineLayout(GpuDevice->GetHandle(), DenoiseCompPipelineLayout, nullptr);
    vkDestroyPipeline(GpuDevice->GetHandle(), DenoisePipeline, nullptr);
}

bool Renderer::RecreateSizedResources()
{
    if (!MainWindow->WaitWhileMinimized())
    {
        return false;
    }

    RTRT_CHECK_VK(vkDeviceWaitIdle(GpuDevice->GetHandle()));
    MainWindow->SetFramebufferResized(false);

    DestroySizedResources();

    CreateSwapchain();
    CreateDepthResource();
    CreatePostFrameBuffers();
    CreateScBuffer();
    CreateScanlineFramebuffer();
    CreateRtBuffers();
    CreateDenoiseBuffer();

    WriteSizedDescriptors();

    // Both graphics pipelines bake in the viewport size
    CreatePostPipeline();
    CreateScPipeline();

    ResetRtAccumulation();
    return true;
}

void Renderer::DestroySizedResources()
{
    for (auto& Framebuffer : Framebuffers)
    {
        vkDestroyFramebuffer(GpuDevice->GetHandle(), Framebuffer, nullptr);
    }
    Framebuffers.clear();
    vkDestroyFramebuffer(GpuDevice->GetHandle(), ScanlineFramebuffer, nullptr);
    DepthImage.Destroy();
    MainSwapchain->Destroy();

    ScImageBuffer.Destroy();
    RtColCurrBuffer.Destroy();
    RtColPrevBuffer.Destroy();
    RtNdCurrBuffer.Destroy();
    RtNdPrevBuffer.Destroy();
    RtKdCurrBuffer.Destroy();
    RtKdPrevBuffer.Destroy();
    DenoiseBuffer.Destroy();

    vkDestroyPipeline(GpuDevice->GetHandle(), PostPipeline, nullptr);
    vkDestroyPipelineLayout(GpuDevice->GetHandle(), PostPipelineLayout, nullptr);
    vkDestroyPipeline(GpuDevice->GetHandle(), ScanlinePipeline, nullptr);
    vkDestroyPipelineLayout(GpuDevice->GetHandle(), ScanlinePipelineLayout, nullptr);
}

void Renderer::WriteSizedDescriptors()
{
    PostDesc.Write(GpuDevice->GetHandle(), 0, ScImageBuffer.Descriptor());

    RtDesc.Write(GpuDevice->GetHandle(), 1, RtColCurrBuffer.Descriptor());
    RtDesc.Write(GpuDevice->GetHandle(), 2, RtColPrevBuffer.Descriptor());
    RtDesc.Write(GpuDevice->GetHandle(), 3, RtNdCurrBuffer.Descriptor());
    RtDesc.Write(GpuDevice->GetHandle(), 4, RtNdPrevBuffer.Descriptor());
    RtDesc.Write(GpuDevice->GetHandle(), 5, RtKdCurrBuffer.Descriptor());
    RtDesc.Write(GpuDevice->GetHandle(), 6, RtKdPrevBuffer.Descriptor());

    DenoiseDesc.Write(GpuDevice->GetHandle(), 0, ScImageBuffer.Descriptor());
    DenoiseDesc.Write(GpuDevice->GetHandle(), 1, DenoiseBuffer.Descriptor());
    DenoiseDesc.Write(GpuDevice->GetHandle(), 2, RtKdCurrBuffer.Descriptor());
    DenoiseDesc.Write(GpuDevice->GetHandle(), 3, RtNdCurrBuffer.Descriptor());
}

// Create a command pool, used to allocate command buffers, which in
// turn are use to gather and send commands to the GPU.  The flag
// makes it possible to reuse command buffers.  The queue index
// determines which queue the command buffers can be submitted to.
// Use the command pool to also create a command buffer.
void Renderer::CreateCommandPool()
{
    MainCmdPool.Create(*GpuDevice);
    FrameCmd.Allocate(MainCmdPool);
}

void Renderer::CreateSwapchain()
{
    MainSwapchain->Create(*GpuDevice, GpuContext->GetSurface(), *MainWindow);

    VkCommandBuffer Cmd = CreateTempCmdBuffer();
    MainSwapchain->RecordInitialTransition(Cmd);
    SubmitTempCmdBuffer(Cmd);
}

void Renderer::CreateFrameFence()
{
    FrameFence.Create(*GpuDevice, true);
}

void Renderer::CreateDepthResource()
{
    uint32_t MipLevels = 1;

    DepthImage = CreateImage(
        MainSwapchain->GetExtent().width, MainSwapchain->GetExtent().height,
        VK_FORMAT_X8_D24_UNORM_PACK32, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MipLevels);
    DepthImage.CreateView(VK_FORMAT_X8_D24_UNORM_PACK32, VK_IMAGE_ASPECT_DEPTH_BIT);
}

// Gets a list of memory types supported by the GPU, and search
// through that list for one that matches the requested properties
// flag.  The (only?) two types requested here are:
//
// (1) VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: For the bulk of the memory
// used by the GPU to store things internally.
//
// (2) VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:
// for memory visible to the CPU  for CPU to GPU copy operations.
Image Renderer::CreateImage(uint32_t Width, uint32_t Height, VkFormat Format,
                            VkImageUsageFlags Usage, VkMemoryPropertyFlags Properties,
                            uint32_t MipLevels)
{
    Image Result;
    Result.Create(*GpuDevice, Width, Height, Format, Usage, Properties, MipLevels);
    return Result;
}

void Renderer::CreatePostRenderPass()
{
    std::array<VkAttachmentDescription, 2> Attachments{};
    // Color attachment
    Attachments[0].format = VK_FORMAT_B8G8R8A8_UNORM;
    Attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    Attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;

    // Depth attachment
    Attachments[1].format = VK_FORMAT_X8_D24_UNORM_PACK32;
    Attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    Attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;

    const VkAttachmentReference ColorReference{0,
                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference DepthReference{
        1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    std::array<VkSubpassDependency, 1> SubpassDependencies{};
    // Transition from final to initial (VK_SUBPASS_EXTERNAL refers to all commands
    // executed outside of the actual renderpass)
    SubpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    SubpassDependencies[0].dstSubpass = 0;
    SubpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    SubpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    SubpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    SubpassDependencies[0].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    SubpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkSubpassDescription SubpassDescription{};
    SubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SubpassDescription.colorAttachmentCount = 1;
    SubpassDescription.pColorAttachments = &ColorReference;
    SubpassDescription.pDepthStencilAttachment = &DepthReference;

    VkRenderPassCreateInfo RenderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    RenderPassInfo.attachmentCount = static_cast<uint32_t>(Attachments.size());
    RenderPassInfo.pAttachments = Attachments.data();
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &SubpassDescription;
    RenderPassInfo.dependencyCount = static_cast<uint32_t>(SubpassDependencies.size());
    RenderPassInfo.pDependencies = SubpassDependencies.data();

    RTRT_CHECK_VK(vkCreateRenderPass(GpuDevice->GetHandle(), &RenderPassInfo, nullptr,
                                     &PostRenderPass));
}

// A VkFrameBuffer wraps several images into a render target --
// usually a color buffer and a depth buffer.
void Renderer::CreatePostFrameBuffers()
{
    std::array<VkImageView, 2> Fbattachments{};

    // Create frame buffers for every swap chain image
    VkFramebufferCreateInfo FramebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    FramebufferInfo.renderPass = PostRenderPass;
    FramebufferInfo.width = MainSwapchain->GetExtent().width;
    FramebufferInfo.height = MainSwapchain->GetExtent().height;
    FramebufferInfo.layers = 1;
    FramebufferInfo.attachmentCount = 2;
    FramebufferInfo.pAttachments = Fbattachments.data();

    // Each of the three swapchain images gets an associated frame
    // buffer, all sharing one depth buffer.
    Framebuffers.resize(MainSwapchain->GetImageCount());
    for (uint32_t I = 0; I < MainSwapchain->GetImageCount(); I++)
    {
        // A color attachment from the swap chain
        Fbattachments[0] = MainSwapchain->GetView(I);
        Fbattachments[1] = DepthImage.GetView();  // A depth attachment
        RTRT_CHECK_VK(vkCreateFramebuffer(GpuDevice->GetHandle(), &FramebufferInfo,
                                          nullptr, &Framebuffers[I]));
    }
}

void Renderer::CreatePostPipeline()
{
    // Creating the pipeline layout
    VkPipelineLayoutCreateInfo CreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    CreateInfo.setLayoutCount = 1;
    CreateInfo.pSetLayouts = &PostDesc.GetLayout();

    VkPushConstantRange PushConstant{VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                     sizeof(PushConstantPost)};
    CreateInfo.pushConstantRangeCount = 1;
    CreateInfo.pPushConstantRanges = &PushConstant;

    RTRT_CHECK_VK(vkCreatePipelineLayout(GpuDevice->GetHandle(), &CreateInfo, nullptr,
                                         &PostPipelineLayout));

    // No geometry in this pipeline's draw.
    VkPipelineVertexInputStateCreateInfo VertexInputInfo{};
    VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputInfo.vertexBindingDescriptionCount = 0;
    VertexInputInfo.pVertexBindingDescriptions = nullptr;
    VertexInputInfo.vertexAttributeDescriptionCount = 0;
    VertexInputInfo.pVertexAttributeDescriptions = nullptr;

    const GraphicsShaders Shaders{"spv/post.vert.spv", "spv/post.vert.slang.spv",
                                  "spv/post.frag.spv", "spv/post.frag.slang.spv"};
    PostPipeline = CreateGraphicsPipeline(Shaders, VertexInputInfo, PostPipelineLayout,
                                          PostRenderPass);
}

VkPipeline Renderer::CreateGraphicsPipeline(
    const GraphicsShaders& Shaders,
    const VkPipelineVertexInputStateCreateInfo& VertexInput, VkPipelineLayout Layout,
    VkRenderPass RenderPass)
{
    // The pipeline keeps compiled copies, so the modules are freed when this returns.
    Shader Vert;
    Vert.Load(GpuDevice->GetHandle(), Shaders.VertGlsl, Shaders.VertSlang, UseSlang);
    Shader Frag;
    Frag.Load(GpuDevice->GetHandle(), Shaders.FragGlsl, Shaders.FragSlang, UseSlang);

    VkPipelineShaderStageCreateInfo ShaderStages[] = {
        Vert.StageInfo(VK_SHADER_STAGE_VERTEX_BIT),
        Frag.StageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)};

    // The state points into itself, so it's wired up here rather than copied around.
    FixedFunctionState State = MakeFixedFunctionState();
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
    PipelineInfo.renderPass = RenderPass;
    PipelineInfo.subpass = 0;
    PipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline Pipeline = VK_NULL_HANDLE;
    RTRT_CHECK_VK(vkCreateGraphicsPipelines(GpuDevice->GetHandle(), VK_NULL_HANDLE, 1,
                                            &PipelineInfo, nullptr, &Pipeline));
    return Pipeline;
}

Renderer::FixedFunctionState Renderer::MakeFixedFunctionState() const
{
    FixedFunctionState State;

    State.InputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    State.InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    State.InputAssembly.primitiveRestartEnable = VK_FALSE;

    State.Viewport.x = 0.0f;
    State.Viewport.y = 0.0f;
    State.Viewport.width = static_cast<float>(MainSwapchain->GetExtent().width);
    State.Viewport.height = static_cast<float>(MainSwapchain->GetExtent().height);
    State.Viewport.minDepth = 0.0f;
    State.Viewport.maxDepth = 1.0f;

    State.Scissor.offset = {0, 0};
    State.Scissor.extent =
        VkExtent2D{MainSwapchain->GetExtent().width, MainSwapchain->GetExtent().height};

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

void Renderer::SetShaderLanguage(bool UseSlangShaders)
{
    if (UseSlangShaders == UseSlang)
    {
        return;
    }
    UseSlang = UseSlangShaders;

    // The last frame may still be using the old pipelines
    RTRT_CHECK_VK(vkDeviceWaitIdle(GpuDevice->GetHandle()));

    vkDestroyPipeline(GpuDevice->GetHandle(), PostPipeline, nullptr);
    vkDestroyPipelineLayout(GpuDevice->GetHandle(), PostPipelineLayout, nullptr);
    CreatePostPipeline();

    vkDestroyPipeline(GpuDevice->GetHandle(), ScanlinePipeline, nullptr);
    vkDestroyPipelineLayout(GpuDevice->GetHandle(), ScanlinePipelineLayout, nullptr);
    CreateScPipeline();

    // The SBT holds group handles of the old pipeline, so it is rebuilt with it
    ShaderBindingTableBw.Destroy();
    vkDestroyPipeline(GpuDevice->GetHandle(), RtPipeline, nullptr);
    vkDestroyPipelineLayout(GpuDevice->GetHandle(), RtPipelineLayout, nullptr);
    CreateRtPipeline();
    CreateRtShaderBindingTable();

    vkDestroyPipeline(GpuDevice->GetHandle(), DenoisePipeline, nullptr);
    vkDestroyPipelineLayout(GpuDevice->GetHandle(), DenoiseCompPipelineLayout, nullptr);
    CreateDenoiseCompPipeline();
}

//----------------------------------------------------------------------------------------
// Post processing pass: tone mapper, UI
void Renderer::PostProcess()
{
    std::array<VkClearValue, 2> ClearValues{};
    ClearValues[0].color = {{1, 1, 1, 1}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo PassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    PassBeginInfo.clearValueCount = 2;
    PassBeginInfo.pClearValues = ClearValues.data();
    PassBeginInfo.renderPass = PostRenderPass;
    PassBeginInfo.framebuffer = Framebuffers[MainSwapchain->GetImageIndex()];
    PassBeginInfo.renderArea = {{0, 0}, MainSwapchain->GetExtent()};

    vkCmdBeginRenderPass(FrameCmd.GetHandle(), &PassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);
    {  // extra indent for renderpass commands

        auto AspectRatio = static_cast<float>(MainSwapchain->GetExtent().width) /
                           static_cast<float>(MainSwapchain->GetExtent().height);
        vkCmdBindPipeline(FrameCmd.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                          PostPipeline);
        vkCmdBindDescriptorSets(FrameCmd.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                PostPipelineLayout, 0, 1, &PostDesc.GetSet(), 0, nullptr);

        // The rasterizer has no G-buffer images, so the debug views only apply to the ray
        // tracer.
        PushConstantPost PcPost{
            Settings.Exposure, Settings.Gamma,
            static_cast<uint32_t>(Settings.UseRaytracer ? Settings.ViewMode
                                                        : kViewFinal)};
        vkCmdPushConstants(FrameCmd.GetHandle(), PostPipelineLayout,
                           VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantPost),
                           &PcPost);

        // Full-screen triangle: the vertex shader generates positions from gl_VertexIndex
        vkCmdDraw(FrameCmd.GetHandle(), 3, 1, 0, 0);

        ImGui::Render();  // Rendering UI
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), FrameCmd.GetHandle());
    }
    vkCmdEndRenderPass(FrameCmd.GetHandle());
}

}  // namespace rtrt
