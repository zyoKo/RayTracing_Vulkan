#ifndef RTRT_SRC_VK_DESCRIPTOR_BUILDER_H_
#define RTRT_SRC_VK_DESCRIPTOR_BUILDER_H_

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "vk/descriptor_pool.h"
#include "vk/descriptor_set.h"
#include "vk/descriptor_set_layout.h"

namespace rtrt
{

// The layout, pool and single set that one pass binds.
struct DescriptorBundle
{
    DescriptorSetLayout Layout;
    DescriptorPool Pool;
    DescriptorSet Set;

    void Destroy()
    {
        Pool.Destroy();
        Layout.Destroy();
    }
};

// Collects bindings in order, starting at binding 0, then builds a DescriptorBundle:
//   DescriptorBuilder().AddUniformBuffer(Stages).AddStorageImage(Stages).Build(...)
class DescriptorBuilder
{
public:
    DescriptorBuilder& AddUniformBuffer(VkShaderStageFlags Stages);
    DescriptorBuilder& AddStorageBuffer(VkShaderStageFlags Stages);
    DescriptorBuilder& AddCombinedImageSampler(VkShaderStageFlags Stages,
                                               uint32_t Count = 1);
    DescriptorBuilder& AddStorageImage(VkShaderStageFlags Stages);
    DescriptorBuilder& AddAccelerationStructure(VkShaderStageFlags Stages);

    // Creates Out's layout, a pool with room for one set, and the set.
    void Build(VkDevice LogicalDevice, DescriptorBundle& Out) const;

private:
    DescriptorBuilder& Add(VkDescriptorType Type, VkShaderStageFlags Stages,
                           uint32_t Count);

    std::vector<VkDescriptorSetLayoutBinding> Bindings;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_DESCRIPTOR_BUILDER_H_
