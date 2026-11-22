#ifndef RTRT_SRC_RENDERER_UI_PASS_H_
#define RTRT_SRC_RENDERER_UI_PASS_H_

#include <vector>

#include <vulkan/vulkan_core.h>

#include "renderer/pass_context.h"
#include "vk/framebuffer.h"
#include "vk/render_pass.h"

namespace rtrt
{

// Clears the acquired swapchain image and draws the ImGui frame into it.
class UiPass
{
public:
    // Creates the render pass and one framebuffer per swapchain image.
    void Create(const PassContext& Context);

    // Frees the framebuffers, before the swapchain is rebuilt.
    void ReleaseSized();
    // Rebuilds them for the new swapchain.
    void OnResize(const PassContext& Context);

    void Destroy();

    // Ends the ImGui frame and records its draw data into the acquired image.
    void Record(VkCommandBuffer Cmd, const PassContext& Context) const;

    VkRenderPass GetRenderPass() const
    {
        return Pass.GetHandle();
    }

private:
    void CreateRenderPass(VkDevice LogicalDevice);
    void CreateFramebuffers(const PassContext& Context);

    RenderPass Pass;
    // One per swapchain image
    std::vector<Framebuffer> Framebuffers;
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_UI_PASS_H_
