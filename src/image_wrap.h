#ifndef RTRT_SRC_IMAGE_WRAP_H_
#define RTRT_SRC_IMAGE_WRAP_H_

#include <vulkan/vulkan_core.h>

namespace rtrt
{

struct ImageWrap
{
    VkImage Image{};
    VkDeviceMemory Memory{};
    VkSampler Sampler{};
    VkImageView ImageView{};
    VkImageLayout ImageLayout{};

    void Destroy(VkDevice Device)
    {
        vkDestroyImage(Device, Image, nullptr);
        vkFreeMemory(Device, Memory, nullptr);
        vkDestroyImageView(Device, ImageView, nullptr);
        vkDestroySampler(Device, Sampler, nullptr);
    }

    VkDescriptorImageInfo Descriptor() const
    {
        return VkDescriptorImageInfo({Sampler, ImageView, ImageLayout});
    }
};

}  // namespace rtrt

#endif  // RTRT_SRC_IMAGE_WRAP_H_
