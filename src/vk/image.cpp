#include "vk/image.h"

#include <utility>

#include "vk_check.h"

namespace rtrt
{

Image::~Image()
{
    Destroy();
}

Image::Image(Image&& Other) noexcept
    : DeviceHandle(std::exchange(Other.DeviceHandle, VK_NULL_HANDLE)),
      Handle(std::exchange(Other.Handle, VK_NULL_HANDLE)),
      Memory(std::exchange(Other.Memory, VK_NULL_HANDLE)),
      View(std::exchange(Other.View, VK_NULL_HANDLE)),
      ImageSampler(std::move(Other.ImageSampler)),
      Layout(std::exchange(Other.Layout, VK_IMAGE_LAYOUT_UNDEFINED))
{
}

Image& Image::operator=(Image&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        DeviceHandle = std::exchange(Other.DeviceHandle, VK_NULL_HANDLE);
        Handle = std::exchange(Other.Handle, VK_NULL_HANDLE);
        Memory = std::exchange(Other.Memory, VK_NULL_HANDLE);
        View = std::exchange(Other.View, VK_NULL_HANDLE);
        ImageSampler = std::move(Other.ImageSampler);
        Layout = std::exchange(Other.Layout, VK_IMAGE_LAYOUT_UNDEFINED);
    }
    return *this;
}

void Image::Create(const Device& Gpu, uint32_t Width, uint32_t Height, VkFormat Format,
                   VkImageUsageFlags Usage, VkMemoryPropertyFlags Properties,
                   uint32_t MipLevels)
{
    Destroy();
    DeviceHandle = Gpu.GetHandle();

    VkImageCreateInfo ImageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageInfo.extent.width = Width;
    ImageInfo.extent.height = Height;
    ImageInfo.extent.depth = 1;
    ImageInfo.mipLevels = MipLevels;
    ImageInfo.arrayLayers = 1;
    ImageInfo.format = Format;
    ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageInfo.usage = Usage;
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    RTRT_CHECK_VK(vkCreateImage(DeviceHandle, &ImageInfo, nullptr, &Handle));

    VkMemoryRequirements MemRequirements;
    vkGetImageMemoryRequirements(DeviceHandle, Handle, &MemRequirements);

    VkMemoryAllocateInfo AllocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    AllocInfo.allocationSize = MemRequirements.size;
    AllocInfo.memoryTypeIndex =
        Gpu.FindMemoryType(MemRequirements.memoryTypeBits, Properties);
    RTRT_CHECK_VK(vkAllocateMemory(DeviceHandle, &AllocInfo, nullptr, &Memory));

    RTRT_CHECK_VK(vkBindImageMemory(DeviceHandle, Handle, Memory, 0));
}

void Image::CreateView(VkFormat Format, VkImageAspectFlags Aspect)
{
    VkImageViewCreateInfo ViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ViewInfo.image = Handle;
    ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ViewInfo.format = Format;
    ViewInfo.subresourceRange.aspectMask = Aspect;
    ViewInfo.subresourceRange.baseMipLevel = 0;
    ViewInfo.subresourceRange.levelCount = 1;
    ViewInfo.subresourceRange.baseArrayLayer = 0;
    ViewInfo.subresourceRange.layerCount = 1;
    RTRT_CHECK_VK(vkCreateImageView(DeviceHandle, &ViewInfo, nullptr, &View));
}

void Image::Destroy()
{
    ImageSampler.Destroy();
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyImageView(DeviceHandle, View, nullptr);
    vkDestroyImage(DeviceHandle, Handle, nullptr);
    vkFreeMemory(DeviceHandle, Memory, nullptr);
    View = VK_NULL_HANDLE;
    Handle = VK_NULL_HANDLE;
    Memory = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
    Layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

}  // namespace rtrt
