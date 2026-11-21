#include "vk/descriptor_set.h"

#include <assert.h>

#include <vector>

namespace rtrt
{

void DescriptorSet::Allocate(VkDevice LogicalDevice, const DescriptorPool& Pool,
                             const DescriptorSetLayout& Layout)
{
    DeviceHandle = LogicalDevice;
    Handle = Pool.Allocate(Layout);

    BindingTypes.clear();
    for (const VkDescriptorSetLayoutBinding& Binding : Layout.GetBindings())
    {
        assert(Binding.binding == BindingTypes.size());
        BindingTypes.push_back(Binding.descriptorType);
    }
}

VkWriteDescriptorSet DescriptorSet::MakeWrite(uint32_t Index, uint32_t Count) const
{
    VkWriteDescriptorSet WriteSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    WriteSet.dstSet = Handle;
    WriteSet.dstBinding = Index;
    WriteSet.dstArrayElement = 0;
    WriteSet.descriptorCount = Count;
    WriteSet.descriptorType = BindingTypes[Index];
    return WriteSet;
}

void DescriptorSet::Write(uint32_t Index, VkBuffer BufferHandle) const
{
    VkDescriptorBufferInfo BufferInfo{BufferHandle, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet WriteSet = MakeWrite(Index, 1);
    WriteSet.pBufferInfo = &BufferInfo;

    assert(WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);

    vkUpdateDescriptorSets(DeviceHandle, 1, &WriteSet, 0, nullptr);
}

void DescriptorSet::Write(uint32_t Index, const VkDescriptorImageInfo& ImageInfo) const
{
    VkWriteDescriptorSet WriteSet = MakeWrite(Index, 1);
    WriteSet.pImageInfo = &ImageInfo;

    assert(WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

    vkUpdateDescriptorSets(DeviceHandle, 1, &WriteSet, 0, nullptr);
}

void DescriptorSet::Write(uint32_t Index, const std::vector<Image>& Textures) const
{
    std::vector<VkDescriptorImageInfo> ImageInfos;
    for (const Image& Texture : Textures)
    {
        ImageInfos.emplace_back(Texture.Descriptor());
    }

    VkWriteDescriptorSet WriteSet =
        MakeWrite(Index, static_cast<uint32_t>(ImageInfos.size()));
    WriteSet.pImageInfo = ImageInfos.data();

    assert(WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

    vkUpdateDescriptorSets(DeviceHandle, 1, &WriteSet, 0, nullptr);
}

void DescriptorSet::Write(uint32_t Index, VkAccelerationStructureKHR Tlas) const
{
    VkWriteDescriptorSetAccelerationStructureKHR AsInfo{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    AsInfo.accelerationStructureCount = 1;
    AsInfo.pAccelerationStructures = &Tlas;

    VkWriteDescriptorSet WriteSet = MakeWrite(Index, 1);
    WriteSet.pNext = &AsInfo;

    assert(WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);

    vkUpdateDescriptorSets(DeviceHandle, 1, &WriteSet, 0, nullptr);
}

}  // namespace rtrt
