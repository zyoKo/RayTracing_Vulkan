#include "renderer/post_pass.h"

#include <array>
#include <cstdint>

#include "renderer/graphics_pipeline.h"

namespace rtrt
{

void PostPass::Create(const PassContext& Context, const Image& Input)
{
    CreateRenderPass(Context.Gpu->GetHandle());
    CreateOutput(Context);

    DescriptorBuilder()
        .AddCombinedImageSampler(VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build(Context.Gpu->GetHandle(), Desc);
    Desc.Set.Write(0, Input.Descriptor());

    CreatePipeline(Context);
}

void PostPass::ReleaseSized()
{
    Target.Destroy();
    Output.Destroy();
    DrawPipeline.Destroy();
    DrawLayout.Destroy();
}

void PostPass::OnResize(const PassContext& Context, const Image& Input)
{
    CreateOutput(Context);
    Desc.Set.Write(0, Input.Descriptor());
    // The pipeline bakes in the viewport size
    CreatePipeline(Context);
}

void PostPass::RebuildPipeline(const PassContext& Context)
{
    DrawPipeline.Destroy();
    DrawLayout.Destroy();
    CreatePipeline(Context);
}

void PostPass::Destroy()
{
    Pass.Destroy();
    Target.Destroy();
    Output.Destroy();
    Desc.Destroy();
    DrawLayout.Destroy();
    DrawPipeline.Destroy();
}

void PostPass::Record(VkCommandBuffer Cmd, const PassContext& Context,
                      const PushConstantPost& Constants) const
{
    VkClearValue ClearValue{};
    ClearValue.color = {{0, 0, 0, 1}};

    VkRenderPassBeginInfo PassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    PassBeginInfo.clearValueCount = 1;
    PassBeginInfo.pClearValues = &ClearValue;
    PassBeginInfo.renderPass = Pass.GetHandle();
    PassBeginInfo.framebuffer = Target.GetHandle();
    PassBeginInfo.renderArea = {{0, 0}, Context.Extent};

    vkCmdBeginRenderPass(Cmd, &PassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, DrawPipeline.GetHandle());
    vkCmdBindDescriptorSets(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, DrawLayout.GetHandle(),
                            0, 1, &Desc.Set.GetHandle(), 0, nullptr);
    vkCmdPushConstants(Cmd, DrawLayout.GetHandle(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstantPost), &Constants);
    // Full-screen triangle: the vertex shader generates positions from gl_VertexIndex
    vkCmdDraw(Cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(Cmd);
}

void PostPass::CreateRenderPass(VkDevice LogicalDevice)
{
    VkAttachmentDescription Attachment{};
    Attachment.format = kOutputFormat;
    Attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    Attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    Attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    Attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    const VkAttachmentReference ColorReference{0,
                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    std::array<VkSubpassDependency, 2> Dependencies{};
    // Last frame's UI draw and screenshot copy read the image before it is cleared
    Dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependencies[0].dstSubpass = 0;
    Dependencies[0].srcStageMask =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    Dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependencies[0].srcAccessMask = 0;
    Dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    // The UI samples the result
    Dependencies[1].srcSubpass = 0;
    Dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    Dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependencies[1].dstStageMask =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    Dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    Dependencies[1].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;

    VkSubpassDescription Subpass{};
    Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    Subpass.colorAttachmentCount = 1;
    Subpass.pColorAttachments = &ColorReference;

    VkRenderPassCreateInfo Info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    Info.attachmentCount = 1;
    Info.pAttachments = &Attachment;
    Info.subpassCount = 1;
    Info.pSubpasses = &Subpass;
    Info.dependencyCount = static_cast<uint32_t>(Dependencies.size());
    Info.pDependencies = Dependencies.data();

    Pass.Create(LogicalDevice, Info);
}

void PostPass::CreateOutput(const PassContext& Context)
{
    Output.Create(*Context.Gpu, Context.Extent.width, Context.Extent.height,
                  kOutputFormat,
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Output.CreateView(kOutputFormat);
    Output.CreateSampler(*Context.Gpu);
    Output.SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // The UI may sample it before the first post draw
    Output.TransitionLayout(*Context.Pool, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    Target.Create(Context.Gpu->GetHandle(), Pass.GetHandle(), {Output.GetView()},
                  Context.Extent);
}

void PostPass::CreatePipeline(const PassContext& Context)
{
    VkPushConstantRange PushConstant{VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                     sizeof(PushConstantPost)};
    DrawLayout.Create(Context.Gpu->GetHandle(), {Desc.Layout.GetHandle()},
                      {PushConstant});

    VkPipelineVertexInputStateCreateInfo VertexInputInfo{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    const GraphicsShaders Shaders{"spv/post.vert.spv", "spv/post.vert.slang.spv",
                                  "spv/post.frag.spv", "spv/post.frag.slang.spv"};
    DrawPipeline = CreateFullWindowPipeline(
        Context.Gpu->GetHandle(), Context.UseSlang, Shaders, VertexInputInfo,
        DrawLayout.GetHandle(), Pass.GetHandle(), Context.Extent);
}

}  // namespace rtrt
