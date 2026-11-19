#ifndef RTRT_SRC_IMAGE_WRAP_H_
#define RTRT_SRC_IMAGE_WRAP_H_

#include <vulkan/vulkan_core.h>

namespace rtrt
{

struct ImageWrap
{
    VkImage image{};
    VkDeviceMemory memory{};
    VkSampler sampler{};
    VkImageView imageView{};
    VkImageLayout imageLayout{};

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

}  // namespace rtrt

#endif  // RTRT_SRC_IMAGE_WRAP_H_
