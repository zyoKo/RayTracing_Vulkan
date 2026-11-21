#include "vk/image.h"

#include <utility>

#include "vk_check.h"

namespace rtrt
{

namespace
{

// Records the blit of level Level-1 into Level, and leaves Level-1 ready for shader
// reads.
void BlitMipLevel(VkCommandBuffer CmdBuffer, VkImage Target,
                  VkImageMemoryBarrier& Barrier, uint32_t Level, int32_t MipWidth,
                  int32_t MipHeight)
{
    Barrier.subresourceRange.baseMipLevel = Level - 1;
    Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    Barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    Barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(CmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &Barrier);

    VkImageBlit Blit{};
    Blit.srcOffsets[0] = {0, 0, 0};
    Blit.srcOffsets[1] = {MipWidth, MipHeight, 1};
    Blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Blit.srcSubresource.mipLevel = Level - 1;
    Blit.srcSubresource.baseArrayLayer = 0;
    Blit.srcSubresource.layerCount = 1;
    Blit.dstOffsets[0] = {0, 0, 0};
    Blit.dstOffsets[1] = {MipWidth > 1 ? MipWidth / 2 : 1,
                          MipHeight > 1 ? MipHeight / 2 : 1, 1};
    Blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Blit.dstSubresource.mipLevel = Level;
    Blit.dstSubresource.baseArrayLayer = 0;
    Blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(CmdBuffer, Target, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Target,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Blit, VK_FILTER_LINEAR);

    Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    Barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(CmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &Barrier);
}

VkAccessFlags AccessFlagsForImageLayout(VkImageLayout Layout)
{
    switch (Layout)
    {
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return VK_ACCESS_HOST_WRITE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        default:
            return VkAccessFlags();
    }
}

VkPipelineStageFlags PipelineStageForLayout(VkImageLayout Layout)
{
    switch (Layout)
    {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;  // Allow queue other than graphic
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;  // Allow queue other than graphic
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return VK_PIPELINE_STAGE_HOST_BIT;
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        default:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
}

}  // namespace

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

void Image::TransitionLayout(const CommandPool& Pool, VkImageLayout OldLayout,
                             VkImageLayout NewLayout, uint32_t MipLevels) const
{
    VkCommandBuffer TempCmdBuffer = Pool.BeginSingleUse();

    VkImageMemoryBarrier Barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    Barrier.oldLayout = OldLayout;
    Barrier.newLayout = NewLayout;
    Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.image = Handle;
    Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Barrier.subresourceRange.baseMipLevel = 0;
    Barrier.subresourceRange.levelCount = MipLevels;
    Barrier.subresourceRange.baseArrayLayer = 0;
    Barrier.subresourceRange.layerCount = 1;
    Barrier.srcAccessMask = 0;
    Barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    VkPipelineStageFlags SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags DestinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdPipelineBarrier(TempCmdBuffer, SourceStage, DestinationStage, 0, 0, nullptr, 0,
                         nullptr, 1, &Barrier);

    Pool.EndSingleUse(TempCmdBuffer);
}

void Image::CopyFrom(const CommandPool& Pool, VkBuffer Source, uint32_t Width,
                     uint32_t Height) const
{
    VkCommandBuffer TempCmdBuffer = Pool.BeginSingleUse();

    VkBufferImageCopy Region{};
    Region.bufferOffset = 0;
    Region.bufferRowLength = 0;
    Region.bufferImageHeight = 0;
    Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Region.imageSubresource.mipLevel = 0;
    Region.imageSubresource.baseArrayLayer = 0;
    Region.imageSubresource.layerCount = 1;
    Region.imageOffset = {0, 0, 0};
    Region.imageExtent = {Width, Height, 1};

    vkCmdCopyBufferToImage(TempCmdBuffer, Source, Handle,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

    Pool.EndSingleUse(TempCmdBuffer);
}

void Image::GenerateMipmaps(const Device& Gpu, const CommandPool& Pool, VkFormat Format,
                            int32_t Width, int32_t Height, uint32_t MipLevels) const
{
    // Check if image format supports linear blitting
    VkFormatProperties FormatProperties;
    vkGetPhysicalDeviceFormatProperties(Gpu.GetPhysical(), Format, &FormatProperties);

    if (!(FormatProperties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        RTRT_FATAL("Texture image format does not support linear blitting");
    }

    VkCommandBuffer TempCmdBuffer = Pool.BeginSingleUse();

    VkImageMemoryBarrier Barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    Barrier.image = Handle;
    Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Barrier.subresourceRange.baseArrayLayer = 0;
    Barrier.subresourceRange.layerCount = 1;
    Barrier.subresourceRange.levelCount = 1;

    int32_t MipWidth = Width;
    int32_t MipHeight = Height;

    for (uint32_t I = 1; I < MipLevels; I++)
    {
        BlitMipLevel(TempCmdBuffer, Handle, Barrier, I, MipWidth, MipHeight);

        if (MipWidth > 1)
        {
            MipWidth /= 2;
        }
        if (MipHeight > 1)
        {
            MipHeight /= 2;
        }
    }

    // The last level was only written to, never blitted from.
    Barrier.subresourceRange.baseMipLevel = MipLevels - 1;
    Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    Barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(TempCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &Barrier);

    Pool.EndSingleUse(TempCmdBuffer);
}

void CmdImageBarrier(VkCommandBuffer Cmdbuffer, VkImage TargetImage,
                     VkImageLayout OldImageLayout, VkImageLayout NewImageLayout,
                     VkImageAspectFlags AspectMask)
{
    VkImageSubresourceRange SubresourceRange;
    SubresourceRange.aspectMask = AspectMask;
    SubresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    SubresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    SubresourceRange.baseMipLevel = 0;
    SubresourceRange.baseArrayLayer = 0;

    VkImageMemoryBarrier ImageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    ImageMemoryBarrier.oldLayout = OldImageLayout;
    ImageMemoryBarrier.newLayout = NewImageLayout;
    ImageMemoryBarrier.image = TargetImage;
    ImageMemoryBarrier.subresourceRange = SubresourceRange;
    ImageMemoryBarrier.srcAccessMask = AccessFlagsForImageLayout(OldImageLayout);
    ImageMemoryBarrier.dstAccessMask = AccessFlagsForImageLayout(NewImageLayout);
    ImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    VkPipelineStageFlags SrcStageMask = PipelineStageForLayout(OldImageLayout);
    VkPipelineStageFlags DestStageMask = PipelineStageForLayout(NewImageLayout);

    vkCmdPipelineBarrier(Cmdbuffer, SrcStageMask, DestStageMask, 0, 0, nullptr, 0,
                         nullptr, 1, &ImageMemoryBarrier);
}

void CmdCopyImage(VkCommandBuffer Cmd, const Image& Src, const Image& Dst,
                  VkExtent2D Size)
{
    VkImageCopy ImageCopyRegion{};
    ImageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ImageCopyRegion.srcSubresource.layerCount = 1;
    ImageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ImageCopyRegion.dstSubresource.layerCount = 1;
    ImageCopyRegion.extent.width = Size.width;
    ImageCopyRegion.extent.height = Size.height;
    ImageCopyRegion.extent.depth = 1;

    CmdImageBarrier(Cmd, Src.GetHandle(), VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    CmdImageBarrier(Cmd, Dst.GetHandle(), VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdCopyImage(Cmd, Src.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   Dst.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                   &ImageCopyRegion);

    CmdImageBarrier(Cmd, Src.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_GENERAL);
    CmdImageBarrier(Cmd, Dst.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_GENERAL);
}

}  // namespace rtrt
