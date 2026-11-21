#ifndef RTRT_SRC_RENDERER_GRAPHICS_PIPELINE_H_
#define RTRT_SRC_RENDERER_GRAPHICS_PIPELINE_H_

#include <vulkan/vulkan_core.h>

#include "vk/pipeline.h"

namespace rtrt
{

// Slang and generated-GLSL SPIR-V files for a vertex and a fragment shader.
struct GraphicsShaders
{
    const char* VertGlsl;
    const char* VertSlang;
    const char* FragGlsl;
    const char* FragSlang;
};

// A triangle-list pipeline drawing to all of Size, with a depth test and no blending.
// Size is baked in, so the pipeline is rebuilt when the window changes size.
Pipeline CreateFullWindowPipeline(VkDevice LogicalDevice, bool UseSlang,
                                  const GraphicsShaders& Shaders,
                                  const VkPipelineVertexInputStateCreateInfo& VertexInput,
                                  VkPipelineLayout Layout, VkRenderPass Pass,
                                  VkExtent2D Size);

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_GRAPHICS_PIPELINE_H_
