#include "vkapp.h"
#include "descriptor_wrap.h"
#include <assert.h>

void DescriptorWrap::setBindings(const VkDevice device, std::vector<VkDescriptorSetLayoutBinding> _bt)
{
    uint maxSets = 1;  // 1 is good enough for us.  In general, may want more;
    bindingTable = _bt;

    // Build descSetLayout
    VkDescriptorSetLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    createInfo.bindingCount = uint32_t(bindingTable.size());
    createInfo.pBindings = bindingTable.data();
    createInfo.flags = 0;
    createInfo.pNext = nullptr;

    VkDescriptorSetLayout descriptorSetLayout;
    vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descSetLayout);

    // Collect the size required for each descriptorType into a vector of poolSizes
    std::vector<VkDescriptorPoolSize> poolSizes;
    
    for (auto it = bindingTable.cbegin(); it != bindingTable.cend(); ++it)  {
        bool found = false;
        for (auto itpool = poolSizes.begin(); itpool != poolSizes.end(); ++itpool) {
            if(itpool->type == it->descriptorType) {
                itpool->descriptorCount += it->descriptorCount * maxSets;
                found = true;
                break; } }
    
        if (!found) {
            VkDescriptorPoolSize poolSize;
            poolSize.type            = it->descriptorType;
            poolSize.descriptorCount = it->descriptorCount * maxSets;
            poolSizes.push_back(poolSize); } }

    
    // Build descPool
    VkDescriptorPoolCreateInfo descrPoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    descrPoolInfo.maxSets                    = maxSets;
    descrPoolInfo.poolSizeCount              = poolSizes.size();
    descrPoolInfo.pPoolSizes                 = poolSizes.data();
    descrPoolInfo.flags                      = 0;

    vkCreateDescriptorPool(device, &descrPoolInfo, nullptr, &descPool);

    // Allocate DescriptorSet
    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool              = descPool;
    allocInfo.descriptorSetCount          = 1;
    allocInfo.pSetLayouts                 = &descSetLayout;

    // Warning: The next line creates a single descriptor set from the
    // above pool since that's all our program needs.  This is too
    // restrictive in general, but fine for this program.
    vkAllocateDescriptorSets(device, &allocInfo, &descSet);
}

void DescriptorWrap::destroy(VkDevice device)
{
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descPool, nullptr);
}

void DescriptorWrap::write(VkDevice& device, uint index, const VkBuffer& buffer)
{
    VkDescriptorBufferInfo desBuf{buffer, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet writeSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeSet.dstSet          = descSet;
    writeSet.dstBinding      = index;
    writeSet.dstArrayElement = 0;
    writeSet.descriptorCount = 1;
    writeSet.descriptorType  = bindingTable[index].descriptorType;
    writeSet.pBufferInfo = &desBuf;

    assert(bindingTable[index].binding == index); 

    assert(writeSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
           writeSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
           writeSet.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
           writeSet.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
    
    vkUpdateDescriptorSets(device, 1, &writeSet, 0, nullptr);

}

void DescriptorWrap::write(VkDevice& device, uint index, const VkDescriptorImageInfo& textureDesc)
{
    //VkDescriptorBufferInfo desBuf{nvbuffer.buffer, 0, VK_WHOLE_SIZE};

    VkWriteDescriptorSet writeSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeSet.dstSet          = descSet;
    writeSet.dstBinding      = index;
    writeSet.dstArrayElement = 0;
    writeSet.descriptorCount = 1;
    writeSet.descriptorType  = bindingTable[index].descriptorType;
    writeSet.pImageInfo      = &textureDesc;

    assert(bindingTable[index].binding == index);

    assert(writeSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
           writeSet.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
           writeSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
           writeSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE  ||
           writeSet.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
    
    vkUpdateDescriptorSets(device, 1, &writeSet, 0, nullptr);
}

void DescriptorWrap::write(VkDevice& device, uint index, const std::vector<ImageWrap>& textures)
{
    //VkDescriptorBufferInfo desBuf{nvbuffer.buffer, 0, VK_WHOLE_SIZE};
    std::vector<VkDescriptorImageInfo> des;
    for(auto& texture : textures)
        des.emplace_back(texture.Descriptor());

    VkWriteDescriptorSet writeSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeSet.dstSet          = descSet;
    writeSet.dstBinding      = index;
    writeSet.dstArrayElement = 0;
    writeSet.descriptorCount = des.size();
    writeSet.descriptorType  = bindingTable[index].descriptorType;
    writeSet.pImageInfo = des.data();

    assert(bindingTable[index].binding == index);

    assert(writeSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
           writeSet.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
           writeSet.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
           writeSet.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE  ||
           writeSet.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
    
    vkUpdateDescriptorSets(device, 1, &writeSet, 0, nullptr);
}

void DescriptorWrap::write(VkDevice& device, uint index, const VkAccelerationStructureKHR& tlas)
{
    VkWriteDescriptorSetAccelerationStructureKHR descASInfo{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    descASInfo.accelerationStructureCount = 1;
    descASInfo.pAccelerationStructures    = &tlas;
  
    VkWriteDescriptorSet writeSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeSet.dstSet          = descSet;
    writeSet.dstBinding      = index;
    writeSet.dstArrayElement = 0;
    writeSet.descriptorCount = 1;
    writeSet.descriptorType  = bindingTable[index].descriptorType;
    writeSet.pNext      = &descASInfo;

    assert(bindingTable[index].binding == index);

    assert(writeSet.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
    
    vkUpdateDescriptorSets(device, 1, &writeSet, 0, nullptr);
}
