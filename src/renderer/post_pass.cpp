#include "renderer/post_pass.h"

#include <array>
#include <cstdint>

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include "renderer/graphics_pipeline.h"

namespace rtrt
{

void PostPass::Create(const PassContext& Context, const Image& Input, const Image& Depth)
{
    CreateRenderPass(Context.Gpu->GetHandle());
    CreateFramebuffers(Context, Depth);

    DescriptorBuilder()
        .AddCombinedImageSampler(VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build(Context.Gpu->GetHandle(), Desc);
    Desc.Set.Write(0, Input.Descriptor());

    CreatePipeline(Context);
}

void PostPass::ReleaseSized()
{
    Framebuffers.clear();
    DrawPipeline.Destroy();
    DrawLayout.Destroy();
}

void PostPass::OnResize(const PassContext& Context, const Image& Input,
                        const Image& Depth)
{
    CreateFramebuffers(Context, Depth);
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
    Framebuffers.clear();
    Desc.Destroy();
    DrawLayout.Destroy();
    DrawPipeline.Destroy();
}

void PostPass::Record(VkCommandBuffer Cmd, const PassContext& Context,
                      const PushConstantPost& Constants) const
{
    std::array<VkClearValue, 2> ClearValues{};
    ClearValues[0].color = {{1, 1, 1, 1}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo PassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    PassBeginInfo.clearValueCount = 2;
    PassBeginInfo.pClearValues = ClearValues.data();
    PassBeginInfo.renderPass = Pass.GetHandle();
    PassBeginInfo.framebuffer = Framebuffers[Context.Chain->GetImageIndex()].GetHandle();
    PassBeginInfo.renderArea = {{0, 0}, Context.Chain->GetExtent()};

    vkCmdBeginRenderPass(Cmd, &PassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    {  // extra indent for renderpass commands

        vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, DrawPipeline.GetHandle());
        vkCmdBindDescriptorSets(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                DrawLayout.GetHandle(), 0, 1, &Desc.Set.GetHandle(), 0,
                                nullptr);

        vkCmdPushConstants(Cmd, DrawLayout.GetHandle(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(PushConstantPost), &Constants);

        // Full-screen triangle: the vertex shader generates positions from gl_VertexIndex
        vkCmdDraw(Cmd, 3, 1, 0, 0);

        ImGui::Render();  // Rendering UI
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Cmd);
    }
    vkCmdEndRenderPass(Cmd);
}

void PostPass::CreateRenderPass(VkDevice LogicalDevice)
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

    Pass.Create(LogicalDevice, RenderPassInfo);
}

void PostPass::CreateFramebuffers(const PassContext& Context, const Image& Depth)
{
    // One framebuffer per swapchain image, all sharing one depth buffer
    Framebuffers.resize(Context.Chain->GetImageCount());
    for (uint32_t I = 0; I < Context.Chain->GetImageCount(); I++)
    {
        Framebuffers[I].Create(Context.Gpu->GetHandle(), Pass.GetHandle(),
                               {Context.Chain->GetView(I), Depth.GetView()},
                               Context.Chain->GetExtent());
    }
}

void PostPass::CreatePipeline(const PassContext& Context)
{
    VkPushConstantRange PushConstant{VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                     sizeof(PushConstantPost)};
    DrawLayout.Create(Context.Gpu->GetHandle(), {Desc.Layout.GetHandle()},
                      {PushConstant});

    // No geometry in this pipeline's draw.
    VkPipelineVertexInputStateCreateInfo VertexInputInfo{};
    VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputInfo.vertexBindingDescriptionCount = 0;
    VertexInputInfo.pVertexBindingDescriptions = nullptr;
    VertexInputInfo.vertexAttributeDescriptionCount = 0;
    VertexInputInfo.pVertexAttributeDescriptions = nullptr;

    const GraphicsShaders Shaders{"spv/post.vert.spv", "spv/post.vert.slang.spv",
                                  "spv/post.frag.spv", "spv/post.frag.slang.spv"};
    DrawPipeline = CreateFullWindowPipeline(
        Context.Gpu->GetHandle(), Context.UseSlang, Shaders, VertexInputInfo,
        DrawLayout.GetHandle(), Pass.GetHandle(), Context.Chain->GetExtent());
}

}  // namespace rtrt
