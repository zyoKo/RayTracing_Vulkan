#ifndef RTRT_SRC_VK_PIPELINE_LAYOUT_H_
#define RTRT_SRC_VK_PIPELINE_LAYOUT_H_

#include <vector>

#include <vulkan/vulkan_core.h>

namespace rtrt
{

// The descriptor set layouts and push constant ranges a pipeline uses. Frees itself
// when destroyed.
class PipelineLayout
{
public:
    PipelineLayout() = default;
    ~PipelineLayout();
    PipelineLayout(const PipelineLayout&) = delete;
    PipelineLayout& operator=(const PipelineLayout&) = delete;

    void Create(VkDevice LogicalDevice,
                const std::vector<VkDescriptorSetLayout>& SetLayouts,
                const std::vector<VkPushConstantRange>& PushConstants);

    // Safe to call on an empty layout.
    void Destroy();

    VkPipelineLayout GetHandle() const
    {
        return Handle;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkPipelineLayout Handle = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_PIPELINE_LAYOUT_H_
