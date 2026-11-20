#ifndef RTRT_SRC_DESCRIPTOR_WRAP_H_
#define RTRT_SRC_DESCRIPTOR_WRAP_H_

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "image_wrap.h"

namespace rtrt
{

class DescriptorWrap
{
public:
    DescriptorWrap() = default;
    DescriptorWrap(const DescriptorWrap&) = delete;
    DescriptorWrap& operator=(const DescriptorWrap&) = delete;

    std::vector<VkDescriptorSetLayoutBinding> BindingTable;

    VkDescriptorSetLayout DescSetLayout;
    VkDescriptorPool DescPool;
    VkDescriptorSet DescSet;  // Could be  vector<VkDescriptorSet> for multiple sets;

    void SetBindings(const VkDevice Device,
                     const std::vector<VkDescriptorSetLayoutBinding>& Bindings);
    void Destroy(VkDevice Device);

    // Any data can be written into a descriptor set.  Apparently I need only these few
    // types:
    void Write(VkDevice Device, uint32_t Index, VkBuffer Buffer);
    void Write(VkDevice Device, uint32_t Index, const VkDescriptorImageInfo& TextureDesc);
    void Write(VkDevice Device, uint32_t Index, const std::vector<ImageWrap>& Textures);
    void Write(VkDevice Device, uint32_t Index, VkAccelerationStructureKHR Tlas);
};

}  // namespace rtrt

#endif  // RTRT_SRC_DESCRIPTOR_WRAP_H_
