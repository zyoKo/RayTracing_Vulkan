#ifndef RTRT_SRC_VK_IMAGE_H_
#define RTRT_SRC_VK_IMAGE_H_

#include <cstdint>

#include <vulkan/vulkan_core.h>

#include "vk/device.h"
#include "vk/sampler.h"

namespace rtrt
{

// A 2D VkImage with its memory, an optional view and sampler, and the layout shaders
// see it in. Frees everything when destroyed; moves hand the handles over.
class Image
{
public:
    Image() = default;
    ~Image();
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& Other) noexcept;
    Image& operator=(Image&& Other) noexcept;

    // Creates the image with optimal tiling and binds memory that has all of
    // Properties.
    void Create(const Device& Gpu, uint32_t Width, uint32_t Height, VkFormat Format,
                VkImageUsageFlags Usage, VkMemoryPropertyFlags Properties,
                uint32_t MipLevels = 1);

    // Creates a view of the first mip level.
    void CreateView(VkFormat Format,
                    VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_COLOR_BIT);

    void CreateSampler(const Device& Gpu)
    {
        ImageSampler.Create(Gpu);
    }

    // The layout descriptors report for this image.
    void SetLayout(VkImageLayout NewLayout)
    {
        Layout = NewLayout;
    }

    // Frees everything. Safe to call on an empty image.
    void Destroy();

    // The sampler, view and layout as a descriptor for DescriptorWrap::Write.
    VkDescriptorImageInfo Descriptor() const
    {
        return VkDescriptorImageInfo({ImageSampler.GetHandle(), View, Layout});
    }

    VkImage GetHandle() const
    {
        return Handle;
    }

    VkImageView GetView() const
    {
        return View;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkImage Handle = VK_NULL_HANDLE;
    VkDeviceMemory Memory = VK_NULL_HANDLE;
    VkImageView View = VK_NULL_HANDLE;
    Sampler ImageSampler;
    VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_IMAGE_H_
