
# pragma once

struct ImageWrap
{
    VkImage          image{};
    VkDeviceMemory   memory{};
    VkSampler        sampler{};
    VkImageView      imageView{};
    VkImageLayout    imageLayout{};
    
    void destroy(VkDevice device)
    {
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, memory, nullptr);
        vkDestroyImageView(device, imageView, nullptr);
        vkDestroySampler(device, sampler, nullptr);
    }
    
    VkDescriptorImageInfo Descriptor() const 
    {
        return VkDescriptorImageInfo({sampler, imageView, imageLayout});
    }
};
