#include "vk/descriptor_pool.h"

#include <vector>

#include "vk_check.h"

namespace rtrt
{

DescriptorPool::~DescriptorPool()
{
    Destroy();
}

void DescriptorPool::Create(VkDevice LogicalDevice,
                            const std::vector<VkDescriptorPoolSize>& Sizes,
                            uint32_t MaxSets)
{
    Destroy();
    DeviceHandle = LogicalDevice;

    VkDescriptorPoolCreateInfo PoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    PoolInfo.maxSets = MaxSets;
    PoolInfo.poolSizeCount = static_cast<uint32_t>(Sizes.size());
    PoolInfo.pPoolSizes = Sizes.data();
    RTRT_CHECK_VK(vkCreateDescriptorPool(DeviceHandle, &PoolInfo, nullptr, &Handle));
}

void DescriptorPool::CreateFor(VkDevice LogicalDevice, const DescriptorSetLayout& Layout,
                               uint32_t MaxSets)
{
    // One pool size per descriptor type, adding up the counts of its bindings
    std::vector<VkDescriptorPoolSize> Sizes;
    for (const VkDescriptorSetLayoutBinding& Binding : Layout.GetBindings())
    {
        bool Found = false;
        for (VkDescriptorPoolSize& Size : Sizes)
        {
            if (Size.type == Binding.descriptorType)
            {
                Size.descriptorCount += Binding.descriptorCount * MaxSets;
                Found = true;
                break;
            }
        }
        if (!Found)
        {
            Sizes.push_back({Binding.descriptorType, Binding.descriptorCount * MaxSets});
        }
    }
    Create(LogicalDevice, Sizes, MaxSets);
}

void DescriptorPool::Destroy()
{
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyDescriptorPool(DeviceHandle, Handle, nullptr);
    Handle = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
}

VkDescriptorSet DescriptorPool::Allocate(const DescriptorSetLayout& Layout) const
{
    VkDescriptorSetAllocateInfo AllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    AllocInfo.descriptorPool = Handle;
    AllocInfo.descriptorSetCount = 1;
    AllocInfo.pSetLayouts = &Layout.GetHandle();

    VkDescriptorSet Set = VK_NULL_HANDLE;
    RTRT_CHECK_VK(vkAllocateDescriptorSets(DeviceHandle, &AllocInfo, &Set));
    return Set;
}

}  // namespace rtrt
