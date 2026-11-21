#include "vk/descriptor_builder.h"

namespace rtrt
{

DescriptorBuilder& DescriptorBuilder::AddUniformBuffer(VkShaderStageFlags Stages)
{
    return Add(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Stages, 1);
}

DescriptorBuilder& DescriptorBuilder::AddStorageBuffer(VkShaderStageFlags Stages)
{
    return Add(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Stages, 1);
}

DescriptorBuilder& DescriptorBuilder::AddCombinedImageSampler(VkShaderStageFlags Stages,
                                                              uint32_t Count)
{
    return Add(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Stages, Count);
}

DescriptorBuilder& DescriptorBuilder::AddStorageImage(VkShaderStageFlags Stages)
{
    return Add(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Stages, 1);
}

DescriptorBuilder& DescriptorBuilder::AddAccelerationStructure(VkShaderStageFlags Stages)
{
    return Add(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, Stages, 1);
}

DescriptorBuilder& DescriptorBuilder::Add(VkDescriptorType Type,
                                          VkShaderStageFlags Stages, uint32_t Count)
{
    VkDescriptorSetLayoutBinding Binding{};
    Binding.binding = static_cast<uint32_t>(Bindings.size());
    Binding.descriptorType = Type;
    Binding.descriptorCount = Count;
    Binding.stageFlags = Stages;
    Bindings.push_back(Binding);
    return *this;
}

void DescriptorBuilder::Build(VkDevice LogicalDevice, DescriptorBundle& Out) const
{
    Out.Layout.Create(LogicalDevice, Bindings);
    Out.Pool.CreateFor(LogicalDevice, Out.Layout, 1);
    Out.Set.Allocate(LogicalDevice, Out.Pool, Out.Layout);
}

}  // namespace rtrt
