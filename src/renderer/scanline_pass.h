#ifndef RTRT_SRC_RENDERER_SCANLINE_PASS_H_
#define RTRT_SRC_RENDERER_SCANLINE_PASS_H_

#include <vulkan/vulkan_core.h>

#include "renderer/pass_context.h"
#include "scene/scene.h"
#include "vk/framebuffer.h"
#include "vk/image.h"
#include "vk/pipeline.h"
#include "vk/pipeline_layout.h"
#include "vk/render_pass.h"

namespace rtrt
{

// The rasterizer: draws every mesh instance into the shared color image.
class ScanlinePass
{
public:
    // Creates the render pass, a framebuffer over Output and Depth, and the pipeline,
    // which reads the scene descriptor set laid out by SceneLayout.
    void Create(const PassContext& Context, const Image& Output, const Image& Depth,
                VkDescriptorSetLayout SceneLayout);

    // Frees what depends on the window size, before the swapchain is rebuilt.
    void ReleaseSized();
    // Rebuilds it once Output and Depth have their new size.
    void OnResize(const PassContext& Context, const Image& Output, const Image& Depth,
                  VkDescriptorSetLayout SceneLayout);

    // Rebuilds the pipeline after a shader language switch.
    void RebuildPipeline(const PassContext& Context, VkDescriptorSetLayout SceneLayout);

    void Destroy();

    // Records the draw of every instance in World, with SceneSet bound as set 0.
    void Record(VkCommandBuffer Cmd, const PassContext& Context, VkDescriptorSet SceneSet,
                const Scene& World) const;

private:
    void CreateRenderPass(VkDevice LogicalDevice);
    void CreateFramebuffer(const PassContext& Context, const Image& Output,
                           const Image& Depth);
    void CreatePipeline(const PassContext& Context, VkDescriptorSetLayout SceneLayout);

    RenderPass Pass;
    Framebuffer Target;
    PipelineLayout DrawLayout;
    Pipeline DrawPipeline;
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_SCANLINE_PASS_H_
