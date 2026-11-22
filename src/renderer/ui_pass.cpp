#include "renderer/ui_pass.h"

#include <cstdint>

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

namespace rtrt
{

void UiPass::Create(const PassContext& Context)
{
    CreateRenderPass(Context.Gpu->GetHandle());
    CreateFramebuffers(Context);
}

void UiPass::ReleaseSized()
{
    Framebuffers.clear();
}

void UiPass::OnResize(const PassContext& Context)
{
    CreateFramebuffers(Context);
}

void UiPass::Destroy()
{
    Framebuffers.clear();
    Pass.Destroy();
}

void UiPass::Record(VkCommandBuffer Cmd, const PassContext& Context) const
{
    VkClearValue ClearValue{};
    ClearValue.color = {{0.06f, 0.06f, 0.07f, 1.0f}};

    VkRenderPassBeginInfo PassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    PassBeginInfo.clearValueCount = 1;
    PassBeginInfo.pClearValues = &ClearValue;
    PassBeginInfo.renderPass = Pass.GetHandle();
    PassBeginInfo.framebuffer = Framebuffers[Context.Chain->GetImageIndex()].GetHandle();
    PassBeginInfo.renderArea = {{0, 0}, Context.Chain->GetExtent()};

    vkCmdBeginRenderPass(Cmd, &PassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Cmd);
    vkCmdEndRenderPass(Cmd);
}

void UiPass::CreateRenderPass(VkDevice LogicalDevice)
{
    VkAttachmentDescription Attachment{};
    Attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
    Attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    Attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    Attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    Attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    const VkAttachmentReference ColorReference{0,
                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    // The acquire semaphore is waited on at the color output stage
    VkSubpassDependency Dependency{};
    Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependency.dstSubpass = 0;
    Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependency.srcAccessMask = 0;
    Dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDescription Subpass{};
    Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    Subpass.colorAttachmentCount = 1;
    Subpass.pColorAttachments = &ColorReference;

    VkRenderPassCreateInfo Info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    Info.attachmentCount = 1;
    Info.pAttachments = &Attachment;
    Info.subpassCount = 1;
    Info.pSubpasses = &Subpass;
    Info.dependencyCount = 1;
    Info.pDependencies = &Dependency;

    Pass.Create(LogicalDevice, Info);
}

void UiPass::CreateFramebuffers(const PassContext& Context)
{
    Framebuffers.resize(Context.Chain->GetImageCount());
    for (uint32_t I = 0; I < Context.Chain->GetImageCount(); I++)
    {
        Framebuffers[I].Create(Context.Gpu->GetHandle(), Pass.GetHandle(),
                               {Context.Chain->GetView(I)}, Context.Chain->GetExtent());
    }
}

}  // namespace rtrt
