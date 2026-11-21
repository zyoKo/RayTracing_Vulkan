#ifndef RTRT_SRC_VK_DESCRIPTOR_POOL_H_
#define RTRT_SRC_VK_DESCRIPTOR_POOL_H_

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "vk/descriptor_set_layout.h"

namespace rtrt
{

// A descriptor pool. Frees itself, and every set allocated from it, when destroyed.
class DescriptorPool
{
public:
    DescriptorPool() = default;
    ~DescriptorPool();
    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;

    void Create(VkDevice LogicalDevice, const std::vector<VkDescriptorPoolSize>& Sizes,
                uint32_t MaxSets);

    // A pool with exactly enough room for MaxSets sets of Layout.
    void CreateFor(VkDevice LogicalDevice, const DescriptorSetLayout& Layout,
                   uint32_t MaxSets);

    // Safe to call on an empty pool.
    void Destroy();

    VkDescriptorSet Allocate(const DescriptorSetLayout& Layout) const;

    VkDescriptorPool GetHandle() const
    {
        return Handle;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkDescriptorPool Handle = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_DESCRIPTOR_POOL_H_
