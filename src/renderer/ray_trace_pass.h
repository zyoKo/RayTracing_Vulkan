#ifndef RTRT_SRC_RENDERER_RAY_TRACE_PASS_H_
#define RTRT_SRC_RENDERER_RAY_TRACE_PASS_H_

#include <vector>

#include <vulkan/vulkan_core.h>

#include "renderer/pass_context.h"
#include "rt/shader_binding_table.h"
#include "shaders/shared_structs.h"
#include "vk/descriptor_builder.h"
#include "vk/image.h"
#include "vk/pipeline.h"
#include "vk/pipeline_layout.h"
#include "vk/shader.h"

namespace rtrt
{

// The path tracer: traces one sample per pixel into its color, albedo and
// normal-and-depth images, and keeps last frame's copies as history.
class RayTracePass
{
public:
    // Creates the images, the descriptor set (Tlas and the images), the pipeline, which
    // also binds the scene descriptor set laid out by SceneLayout, and the shader
    // binding table.
    void Create(const PassContext& Context, VkAccelerationStructureKHR Tlas,
                VkDescriptorSetLayout SceneLayout);

    // Frees the images, before the swapchain is rebuilt.
    void ReleaseSized();
    // Rebuilds them at the new swapchain size and rewrites their descriptors.
    void OnResize(const PassContext& Context);

    // Rebuilds the pipeline and its shader binding table after a shader language switch.
    void RebuildPipeline(const PassContext& Context, VkDescriptorSetLayout SceneLayout);

    void Destroy();

    // Traces the frame with SceneSet bound as set 1, copies the image ViewMode shows into
    // Output, and saves this frame as history. Clear throws the old history away.
    void Record(VkCommandBuffer Cmd, const PassContext& Context, VkDescriptorSet SceneSet,
                const Image& Output, int ViewMode, bool Clear);

    PushConstantRay& GetConstants()
    {
        return Constants;
    }

    const Image& GetAlbedo() const
    {
        return KdCurr;
    }

    const Image& GetNormalDepth() const
    {
        return NdCurr;
    }

private:
    void CreateImages(const PassContext& Context);
    void WriteImageDescriptors() const;
    void CreatePipeline(const PassContext& Context, VkDescriptorSetLayout SceneLayout);
    // One raygen, one miss and one closest-hit shader, each in its own group.
    void CollectShaders(const PassContext& Context, std::vector<Shader>& Modules,
                        std::vector<VkPipelineShaderStageCreateInfo>& Stages,
                        std::vector<VkRayTracingShaderGroupCreateInfoKHR>& Groups) const;

    Image ColCurr;
    Image ColPrev;
    // Albedo
    Image KdCurr;
    Image KdPrev;
    // Normal and depth
    Image NdCurr;
    Image NdPrev;
    DescriptorBundle Desc;
    PipelineLayout TraceLayout;
    Pipeline TracePipeline;
    ShaderBindingTable Sbt;
    PushConstantRay Constants{};
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_RAY_TRACE_PASS_H_
