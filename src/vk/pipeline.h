#ifndef RTRT_SRC_VK_PIPELINE_H_
#define RTRT_SRC_VK_PIPELINE_H_

#include <vulkan/vulkan_core.h>

namespace rtrt
{

// A graphics, compute or ray tracing pipeline. Frees itself when destroyed; moves hand
// the handle over.
class Pipeline
{
public:
    Pipeline() = default;
    ~Pipeline();
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&& Other) noexcept;
    Pipeline& operator=(Pipeline&& Other) noexcept;

    // Each creates the pipeline from a filled-in create info, replacing any old one.
    void CreateGraphics(VkDevice LogicalDevice, const VkGraphicsPipelineCreateInfo& Info);
    void CreateCompute(VkDevice LogicalDevice, const VkComputePipelineCreateInfo& Info);
    void CreateRayTracing(VkDevice LogicalDevice,
                          const VkRayTracingPipelineCreateInfoKHR& Info);

    // Safe to call on an empty pipeline.
    void Destroy();

    VkPipeline GetHandle() const
    {
        return Handle;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkPipeline Handle = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_PIPELINE_H_
