#ifndef RTRT_SRC_DESCRIPTOR_WRAP_H_
#define RTRT_SRC_DESCRIPTOR_WRAP_H_

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "image_wrap.h"

namespace rtrt
{

// One descriptor set with its layout and pool.
class DescriptorWrap
{
public:
    DescriptorWrap() = default;
    DescriptorWrap(const DescriptorWrap&) = delete;
    DescriptorWrap& operator=(const DescriptorWrap&) = delete;

    // Creates the layout, a pool and one descriptor set for these bindings.
    void SetBindings(const VkDevice Device,
                     const std::vector<VkDescriptorSetLayoutBinding>& Bindings);
    void Destroy(VkDevice Device);

    // Writes one binding of the set; the binding's type must match the data.
    void Write(VkDevice Device, uint32_t Index, VkBuffer Buffer);
    void Write(VkDevice Device, uint32_t Index, const VkDescriptorImageInfo& TextureDesc);
    void Write(VkDevice Device, uint32_t Index, const std::vector<ImageWrap>& Textures);
    void Write(VkDevice Device, uint32_t Index, VkAccelerationStructureKHR Tlas);

    const VkDescriptorSetLayout& GetLayout() const
    {
        return DescSetLayout;
    }

    const VkDescriptorSet& GetSet() const
    {
        return DescSet;
    }

private:
    std::vector<VkDescriptorSetLayoutBinding> BindingTable;

    VkDescriptorSetLayout DescSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool DescPool{VK_NULL_HANDLE};
    // Could be vector<VkDescriptorSet> for multiple sets.
    VkDescriptorSet DescSet{VK_NULL_HANDLE};
};

}  // namespace rtrt

#endif  // RTRT_SRC_DESCRIPTOR_WRAP_H_
