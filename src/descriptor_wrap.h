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

    std::vector<VkDescriptorSetLayoutBinding> bindingTable;

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorPool descPool;
    VkDescriptorSet descSet;  // Could be  vector<VkDescriptorSet> for multiple sets;

    void setBindings(const VkDevice device,
                     const std::vector<VkDescriptorSetLayoutBinding>& _bt);
    void destroy(VkDevice device);

    // Any data can be written into a descriptor set.  Apparently I need only these few
    // types:
    void write(VkDevice device, uint32_t index, VkBuffer buffer);
    void write(VkDevice device, uint32_t index, const VkDescriptorImageInfo& textureDesc);
    void write(VkDevice device, uint32_t index, const std::vector<ImageWrap>& textures);
    void write(VkDevice device, uint32_t index, VkAccelerationStructureKHR tlas);
};

}  // namespace rtrt

#endif  // RTRT_SRC_DESCRIPTOR_WRAP_H_
