#ifndef RTRT_SRC_RENDERER_POST_PASS_H_
#define RTRT_SRC_RENDERER_POST_PASS_H_

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

// Tone-maps the rendered image into an RGBA8 viewport image that the UI samples.
class PostPass
{
public:
    static constexpr VkFormat kOutputFormat = VK_FORMAT_R8G8B8A8_UNORM;

    // Creates the render pass, the output image and its framebuffer, the descriptor set
    // that samples Input, and the pipeline.
    void Create(const PassContext& Context, const Image& Input);

    // Frees what depends on the render size.
    void ReleaseSized();
    // Rebuilds it once Input has its new size.
    void OnResize(const PassContext& Context, const Image& Input);

    // Rebuilds the pipeline after a shader language switch.
    void RebuildPipeline(const PassContext& Context);

    void Destroy();

    // Records the full-screen draw into the output image, leaving it ready for
    // fragment-shader reads.
    void Record(VkCommandBuffer Cmd, const PassContext& Context,
                const PushConstantPost& Constants) const;

    // In the shader-read layout after Record.
    const Image& GetOutput() const
    {
        return Output;
    }

private:
    void CreateRenderPass(VkDevice LogicalDevice);
    void CreateOutput(const PassContext& Context);
    void CreatePipeline(const PassContext& Context);

    RenderPass Pass;
    Image Output;
    Framebuffer Target;
    DescriptorBundle Desc;
    PipelineLayout DrawLayout;
    Pipeline DrawPipeline;
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_POST_PASS_H_
