#ifndef RTRT_SRC_IMAGE_WRAP_H_
#define RTRT_SRC_IMAGE_WRAP_H_

#include <vulkan/vulkan_core.h>

namespace rtrt
{

// A VkImage with its memory, view, sampler and current layout. Plain data;
// Destroy() frees everything it holds.
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

    // The sampler, view and layout as a descriptor for DescriptorWrap::Write.
    VkDescriptorImageInfo Descriptor() const
    {
        return VkDescriptorImageInfo({Sampler, ImageView, ImageLayout});
    }
};

}  // namespace rtrt

#endif  // RTRT_SRC_IMAGE_WRAP_H_
