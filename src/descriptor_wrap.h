#ifndef RTRT_SRC_DESCRIPTOR_WRAP_H_
#define RTRT_SRC_DESCRIPTOR_WRAP_H_

#include <vector>

#include <vulkan/vulkan_core.h>

#include "image_wrap.h"
#include "shaders/shared_structs.h"

class DescriptorWrap
{
public:
    std::vector<VkDescriptorSetLayoutBinding> bindingTable;

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorPool descPool;
    VkDescriptorSet descSet;  // Could be  vector<VkDescriptorSet> for multiple sets;

    void setBindings(const VkDevice device,
                     std::vector<VkDescriptorSetLayoutBinding> _bt);
    void destroy(VkDevice device);

    // Any data can be written into a descriptor set.  Apparently I need only these few
    // types:
    void write(VkDevice& device, uint index, const VkBuffer& buffer);
    void write(VkDevice& device, uint index, const VkDescriptorImageInfo& textureDesc);
    void write(VkDevice& device, uint index, const std::vector<ImageWrap>& textures);
    void write(VkDevice& device, uint index, const VkAccelerationStructureKHR& tlas);
};

#endif  // RTRT_SRC_DESCRIPTOR_WRAP_H_
