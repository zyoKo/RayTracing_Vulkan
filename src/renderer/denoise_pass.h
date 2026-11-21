#ifndef RTRT_SRC_RENDERER_DENOISE_PASS_H_
#define RTRT_SRC_RENDERER_DENOISE_PASS_H_

#include <vulkan/vulkan_core.h>

#include "renderer/pass_context.h"
#include "shaders/shared_structs.h"
#include "vk/descriptor_builder.h"
#include "vk/image.h"
#include "vk/pipeline.h"
#include "vk/pipeline_layout.h"

namespace rtrt
{

// The A-trous wavelet denoiser: filters the shared color image in place, guided by the
// ray tracer's albedo and normal-and-depth images.
class DenoisePass
{
public:
    // Creates the output image, the descriptor set over Color, Albedo and NormalDepth,
    // and the compute pipeline.
    void Create(const PassContext& Context, const Image& Color, const Image& Albedo,
                const Image& NormalDepth);

    // Frees the output image, before the swapchain is rebuilt.
    void ReleaseSized();
    // Rebuilds it at the new size and rewrites every descriptor.
    void OnResize(const PassContext& Context, const Image& Color, const Image& Albedo,
                  const Image& NormalDepth);

    // Rebuilds the pipeline after a shader language switch.
    void RebuildPipeline(const PassContext& Context);

    void Destroy();

    // Records Iterations filter passes over Color, each with twice the step of the last.
    void Record(VkCommandBuffer Cmd, const PassContext& Context, const Image& Color,
                int Iterations);

    PushConstantDenoise& GetConstants()
    {
        return Constants;
    }

private:
    void CreateOutput(const PassContext& Context);
    void WriteDescriptors(const Image& Color, const Image& Albedo,
                          const Image& NormalDepth) const;
    void CreatePipeline(const PassContext& Context);

    Image Output;
    DescriptorBundle Desc;
    PipelineLayout FilterLayout;
    Pipeline FilterPipeline;
    PushConstantDenoise Constants{};
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_DENOISE_PASS_H_
