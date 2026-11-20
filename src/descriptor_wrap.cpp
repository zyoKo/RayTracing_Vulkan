#include "descriptor_wrap.h"

#include <assert.h>

#include <vector>

#include "vk_check.h"

namespace rtrt
{

void DescriptorWrap::SetBindings(
    const VkDevice Device, const std::vector<VkDescriptorSetLayoutBinding>& Bindings)
{
    uint32_t MaxSets = 1;  // 1 is good enough for us.  In general, may want more;
    BindingTable = Bindings;

    // Build descSetLayout
    VkDescriptorSetLayoutCreateInfo CreateInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    CreateInfo.bindingCount = static_cast<uint32_t>(BindingTable.size());
    CreateInfo.pBindings = BindingTable.data();
    CreateInfo.flags = 0;
    CreateInfo.pNext = nullptr;

    RTRT_CHECK_VK(
        vkCreateDescriptorSetLayout(Device, &CreateInfo, nullptr, &DescSetLayout));

    // Collect the size required for each descriptorType into a vector of poolSizes
    std::vector<VkDescriptorPoolSize> PoolSizes;

    for (auto It = BindingTable.cbegin(); It != BindingTable.cend(); ++It)
    {
        bool Found = false;
        for (auto ItPool = PoolSizes.begin(); ItPool != PoolSizes.end(); ++ItPool)
        {
            if (ItPool->type == It->descriptorType)
            {
                ItPool->descriptorCount += It->descriptorCount * MaxSets;
                Found = true;
                break;
            }
        }

        if (!Found)
        {
            VkDescriptorPoolSize PoolSize;
            PoolSize.type = It->descriptorType;
            PoolSize.descriptorCount = It->descriptorCount * MaxSets;
            PoolSizes.push_back(PoolSize);
        }
    }

    // Build descPool
    VkDescriptorPoolCreateInfo DescrPoolInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    DescrPoolInfo.maxSets = MaxSets;
    DescrPoolInfo.poolSizeCount = static_cast<uint32_t>(PoolSizes.size());
    DescrPoolInfo.pPoolSizes = PoolSizes.data();
    DescrPoolInfo.flags = 0;

    RTRT_CHECK_VK(vkCreateDescriptorPool(Device, &DescrPoolInfo, nullptr, &DescPool));

    // Allocate DescriptorSet
    VkDescriptorSetAllocateInfo AllocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    AllocInfo.descriptorPool = DescPool;
    AllocInfo.descriptorSetCount = 1;
    AllocInfo.pSetLayouts = &DescSetLayout;

    // Warning: The next line creates a single descriptor set from the
    // above pool since that's all our program needs.  This is too
    // restrictive in general, but fine for this program.
    RTRT_CHECK_VK(vkAllocateDescriptorSets(Device, &AllocInfo, &DescSet));
}

void DescriptorWrap::Destroy(VkDevice Device)
{
    vkDestroyDescriptorSetLayout(Device, DescSetLayout, nullptr);
    vkDestroyDescriptorPool(Device, DescPool, nullptr);
}

void DescriptorWrap::Write(VkDevice Device, uint32_t Index, VkBuffer Buffer)
{
    VkDescriptorBufferInfo DesBuf{Buffer, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet WriteSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    WriteSet.dstSet = DescSet;
    WriteSet.dstBinding = Index;
    WriteSet.dstArrayElement = 0;
    WriteSet.descriptorCount = 1;
    WriteSet.descriptorType = BindingTable[Index].descriptorType;
    WriteSet.pBufferInfo = &DesBuf;

    assert(BindingTable[Index].binding == Index);

    assert(WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);

    vkUpdateDescriptorSets(Device, 1, &WriteSet, 0, nullptr);
}

void DescriptorWrap::Write(VkDevice Device, uint32_t Index,
                           const VkDescriptorImageInfo& TextureDesc)
{
    VkWriteDescriptorSet WriteSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    WriteSet.dstSet = DescSet;
    WriteSet.dstBinding = Index;
    WriteSet.dstArrayElement = 0;
    WriteSet.descriptorCount = 1;
    WriteSet.descriptorType = BindingTable[Index].descriptorType;
    WriteSet.pImageInfo = &TextureDesc;

    assert(BindingTable[Index].binding == Index);

    assert(WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

    vkUpdateDescriptorSets(Device, 1, &WriteSet, 0, nullptr);
}

void DescriptorWrap::Write(VkDevice Device, uint32_t Index,
                           const std::vector<ImageWrap>& Textures)
{
    std::vector<VkDescriptorImageInfo> Des;
    for (auto& Texture : Textures)
    {
        Des.emplace_back(Texture.Descriptor());
    }

    VkWriteDescriptorSet WriteSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    WriteSet.dstSet = DescSet;
    WriteSet.dstBinding = Index;
    WriteSet.dstArrayElement = 0;
    WriteSet.descriptorCount = static_cast<uint32_t>(Des.size());
    WriteSet.descriptorType = BindingTable[Index].descriptorType;
    WriteSet.pImageInfo = Des.data();

    assert(BindingTable[Index].binding == Index);

    assert(WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
           WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

    vkUpdateDescriptorSets(Device, 1, &WriteSet, 0, nullptr);
}

void DescriptorWrap::Write(VkDevice Device, uint32_t Index,
                           VkAccelerationStructureKHR Tlas)
{
    VkWriteDescriptorSetAccelerationStructureKHR DescAsInfo{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    DescAsInfo.accelerationStructureCount = 1;
    DescAsInfo.pAccelerationStructures = &Tlas;

    VkWriteDescriptorSet WriteSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    WriteSet.dstSet = DescSet;
    WriteSet.dstBinding = Index;
    WriteSet.dstArrayElement = 0;
    WriteSet.descriptorCount = 1;
    WriteSet.descriptorType = BindingTable[Index].descriptorType;
    WriteSet.pNext = &DescAsInfo;

    assert(BindingTable[Index].binding == Index);

    assert(WriteSet.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);

    vkUpdateDescriptorSets(Device, 1, &WriteSet, 0, nullptr);
}

}  // namespace rtrt
