#include "vk/pipeline_layout.h"

#include <vector>

#include "vk_check.h"

namespace rtrt
{

PipelineLayout::~PipelineLayout()
{
    Destroy();
}

void PipelineLayout::Create(VkDevice LogicalDevice,
                            const std::vector<VkDescriptorSetLayout>& SetLayouts,
                            const std::vector<VkPushConstantRange>& PushConstants)
{
    Destroy();
    DeviceHandle = LogicalDevice;

    VkPipelineLayoutCreateInfo CreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    CreateInfo.setLayoutCount = static_cast<uint32_t>(SetLayouts.size());
    CreateInfo.pSetLayouts = SetLayouts.data();
    CreateInfo.pushConstantRangeCount = static_cast<uint32_t>(PushConstants.size());
    CreateInfo.pPushConstantRanges = PushConstants.data();
    RTRT_CHECK_VK(vkCreatePipelineLayout(DeviceHandle, &CreateInfo, nullptr, &Handle));
}

void PipelineLayout::Destroy()
{
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyPipelineLayout(DeviceHandle, Handle, nullptr);
    Handle = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
}

}  // namespace rtrt
