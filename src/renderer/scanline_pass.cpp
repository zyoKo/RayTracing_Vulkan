#include "renderer/scanline_pass.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "renderer/graphics_pipeline.h"
#include "shaders/shared_structs.h"

namespace rtrt
{

void ScanlinePass::Create(const PassContext& Context, const Image& Output,
                          const Image& Depth, VkDescriptorSetLayout SceneLayout)
{
    CreateRenderPass(Context.Gpu->GetHandle());
    CreateFramebuffer(Context, Output, Depth);
    CreatePipeline(Context, SceneLayout);
}

void ScanlinePass::ReleaseSized()
{
    Target.Destroy();
    DrawPipeline.Destroy();
    DrawLayout.Destroy();
}

void ScanlinePass::OnResize(const PassContext& Context, const Image& Output,
                            const Image& Depth, VkDescriptorSetLayout SceneLayout)
{
    CreateFramebuffer(Context, Output, Depth);
    // The pipeline bakes in the viewport size
    CreatePipeline(Context, SceneLayout);
}

void ScanlinePass::RebuildPipeline(const PassContext& Context,
                                   VkDescriptorSetLayout SceneLayout)
{
    DrawPipeline.Destroy();
    DrawLayout.Destroy();
    CreatePipeline(Context, SceneLayout);
}

void ScanlinePass::Destroy()
{
    Pass.Destroy();
    Target.Destroy();
    DrawLayout.Destroy();
    DrawPipeline.Destroy();
}

void ScanlinePass::Record(VkCommandBuffer Cmd, const PassContext& Context,
                          VkDescriptorSet SceneSet, const Scene& World) const
{
    VkDeviceSize Offset{0};

    std::array<VkClearValue, 2> ClearValues{};
    ClearValues[0].color = {{0, 0, 0, 1}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo PassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    PassBeginInfo.clearValueCount = 2;
    PassBeginInfo.pClearValues = ClearValues.data();
    PassBeginInfo.renderPass = Pass.GetHandle();
    PassBeginInfo.framebuffer = Target.GetHandle();
    PassBeginInfo.renderArea = {{0, 0}, Context.Chain->GetExtent()};
    vkCmdBeginRenderPass(Cmd, &PassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, DrawPipeline.GetHandle());
    vkCmdBindDescriptorSets(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, DrawLayout.GetHandle(),
                            0, 1, &SceneSet, 0, nullptr);

    for (const MeshInstance& Inst : World.GetInstances())
    {
        const Mesh& Object = World.GetMeshes()[Inst.ObjIndex];

        // Information pushed at each draw call
        PushConstantRaster PcRaster{
            Inst.Transform,      // Object's instance transform.
            {0.5f, 2.5f, 3.0f},  // light position;  Should not be hard-coded here!
            Inst.ObjIndex,       // instance Id
            1.0f                 // light intensity;  Should not be hard-coded here!
        };

        PcRaster.ObjIndex = Inst.ObjIndex;  // Telling which object is drawn
        PcRaster.ModelMatrix = Inst.Transform;

        vkCmdPushConstants(Cmd, DrawLayout.GetHandle(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(PushConstantRaster), &PcRaster);
        const VkBuffer VertexBuffer = Object.VertexBuffer.GetHandle();
        vkCmdBindVertexBuffers(Cmd, 0, 1, &VertexBuffer, &Offset);
        vkCmdBindIndexBuffer(Cmd, Object.IndexBuffer.GetHandle(), 0,
                             VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(Cmd, Object.NbIndices, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(Cmd);
}

void ScanlinePass::CreateRenderPass(VkDevice LogicalDevice)
{
    VkAttachmentDescription ColorAttachment{};
    ColorAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentDescription DepthAttachment{};
    DepthAttachment.format = VK_FORMAT_X8_D24_UNORM_PACK32;
    DepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference ColorAttachmentRef{};
    ColorAttachmentRef.attachment = 0;
    ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference DepthAttachmentRef{};
    DepthAttachmentRef.attachment = 1;
    DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription Subpass{};
    Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    Subpass.colorAttachmentCount = 1;
    Subpass.pColorAttachments = &ColorAttachmentRef;
    Subpass.pDepthStencilAttachment = &DepthAttachmentRef;

    VkSubpassDependency Dependency{};
    Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependency.dstSubpass = 0;
    Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    Dependency.srcAccessMask = 0;
    Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> AttachmentsDsc = {ColorAttachment,
                                                             DepthAttachment};
    VkRenderPassCreateInfo RenderPassInfo{};
    RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassInfo.attachmentCount = static_cast<uint32_t>(AttachmentsDsc.size());
    RenderPassInfo.pAttachments = AttachmentsDsc.data();
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &Subpass;
    RenderPassInfo.dependencyCount = 1;
    RenderPassInfo.pDependencies = &Dependency;

    Pass.Create(LogicalDevice, RenderPassInfo);
}

void ScanlinePass::CreateFramebuffer(const PassContext& Context, const Image& Output,
                                     const Image& Depth)
{
    Target.Create(Context.Gpu->GetHandle(), Pass.GetHandle(),
                  {Output.GetView(), Depth.GetView()}, Context.Chain->GetExtent());
}

void ScanlinePass::CreatePipeline(const PassContext& Context,
                                  VkDescriptorSetLayout SceneLayout)
{
    VkPushConstantRange PushConstantRanges = {
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
        sizeof(PushConstantRaster)};

    DrawLayout.Create(Context.Gpu->GetHandle(), {SceneLayout}, {PushConstantRanges});

    VkVertexInputBindingDescription BindingDescription{0, sizeof(Vertex),
                                                       VK_VERTEX_INPUT_RATE_VERTEX};

    std::vector<VkVertexInputAttributeDescription> AttributeDescriptions{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, Pos))},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, Nrm))},
        {2, 0, VK_FORMAT_R32G32_SFLOAT,
         static_cast<uint32_t>(offsetof(Vertex, TexCoord))}};

    VkPipelineVertexInputStateCreateInfo VertexInputInfo{};
    VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputInfo.vertexBindingDescriptionCount = 1;
    VertexInputInfo.pVertexBindingDescriptions = &BindingDescription;
    VertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(AttributeDescriptions.size());
    VertexInputInfo.pVertexAttributeDescriptions = AttributeDescriptions.data();

    const GraphicsShaders Shaders{"spv/scanline.vert.spv", "spv/scanline.vert.slang.spv",
                                  "spv/scanline.frag.spv", "spv/scanline.frag.slang.spv"};
    DrawPipeline = CreateFullWindowPipeline(
        Context.Gpu->GetHandle(), Context.UseSlang, Shaders, VertexInputInfo,
        DrawLayout.GetHandle(), Pass.GetHandle(), Context.Chain->GetExtent());
}

}  // namespace rtrt
