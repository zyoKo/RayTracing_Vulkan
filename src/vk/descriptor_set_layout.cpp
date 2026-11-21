#include "vk/descriptor_set_layout.h"

#include <vector>

#include "vk_check.h"

namespace rtrt
{

DescriptorSetLayout::~DescriptorSetLayout()
{
    Destroy();
}

void DescriptorSetLayout::Create(
    VkDevice LogicalDevice, const std::vector<VkDescriptorSetLayoutBinding>& NewBindings)
{
    Destroy();
    DeviceHandle = LogicalDevice;
    Bindings = NewBindings;

    VkDescriptorSetLayoutCreateInfo CreateInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    CreateInfo.bindingCount = static_cast<uint32_t>(Bindings.size());
    CreateInfo.pBindings = Bindings.data();
    RTRT_CHECK_VK(
        vkCreateDescriptorSetLayout(DeviceHandle, &CreateInfo, nullptr, &Handle));
}

void DescriptorSetLayout::Destroy()
{
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyDescriptorSetLayout(DeviceHandle, Handle, nullptr);
    Handle = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
    Bindings.clear();
}

}  // namespace rtrt
