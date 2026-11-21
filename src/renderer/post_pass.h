#ifndef RTRT_SRC_RENDERER_POST_PASS_H_
#define RTRT_SRC_RENDERER_POST_PASS_H_

#include <vector>

#include <vulkan/vulkan_core.h>

#include "renderer/pass_context.h"
#include "shaders/shared_structs.h"
#include "vk/descriptor_builder.h"
#include "vk/framebuffer.h"
#include "vk/image.h"
#include "vk/pipeline.h"
#include "vk/pipeline_layout.h"
#include "vk/render_pass.h"

namespace rtrt
{

// Tone-maps the rendered image into the swapchain image and draws the UI on top.
class PostPass
{
public:
    // Creates the render pass, one framebuffer per swapchain image (sharing Depth), the
    // descriptor set that samples Input, and the pipeline.
    void Create(const PassContext& Context, const Image& Input, const Image& Depth);

    // Frees what depends on the window size, before the swapchain is rebuilt.
    void ReleaseSized();
    // Rebuilds it once the swapchain, Input and Depth have their new size.
    void OnResize(const PassContext& Context, const Image& Input, const Image& Depth);

    // Rebuilds the pipeline after a shader language switch.
    void RebuildPipeline(const PassContext& Context);

    void Destroy();

    // Records the full-screen draw into the acquired swapchain image, then the ImGui
    // draw data.
    void Record(VkCommandBuffer Cmd, const PassContext& Context,
                const PushConstantPost& Constants) const;

    VkRenderPass GetRenderPass() const
    {
        return Pass.GetHandle();
    }

private:
    void CreateRenderPass(VkDevice LogicalDevice);
    void CreateFramebuffers(const PassContext& Context, const Image& Depth);
    void CreatePipeline(const PassContext& Context);

    RenderPass Pass;
    // One per swapchain image
    std::vector<Framebuffer> Framebuffers;
    DescriptorBundle Desc;
    PipelineLayout DrawLayout;
    Pipeline DrawPipeline;
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_POST_PASS_H_
