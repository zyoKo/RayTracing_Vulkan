#include "renderer/screenshot.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace rtrt
{

namespace fs = std::filesystem;

void Screenshot::Record(VkCommandBuffer Cmd, const Device& Gpu, const Image& Source,
                        VkExtent2D Size)
{
    if (RequestedPath.empty() || !InFlightPath.empty())
    {
        return;
    }

    const VkDeviceSize Bytes = static_cast<VkDeviceSize>(Size.width) * Size.height * 4;
    Staging.Create(
        Gpu, Bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    CmdImageBarrier(Cmd, Source.GetHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy Region{};
    Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Region.imageSubresource.layerCount = 1;
    Region.imageExtent = {Size.width, Size.height, 1};
    vkCmdCopyImageToBuffer(Cmd, Source.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           Staging.GetHandle(), 1, &Region);
    CmdImageBarrier(Cmd, Source.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkBufferMemoryBarrier HostRead{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    HostRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    HostRead.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    HostRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    HostRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    HostRead.buffer = Staging.GetHandle();
    HostRead.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(Cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                         0, 0, nullptr, 1, &HostRead, 0, nullptr);

    InFlightPath = RequestedPath;
    InFlightSize = Size;
    RequestedPath.clear();
}

void Screenshot::Resolve()
{
    if (InFlightPath.empty())
    {
        return;
    }

    const fs::path Target(InFlightPath);
    std::error_code Error;
    if (Target.has_parent_path())
    {
        fs::create_directories(Target.parent_path(), Error);
    }

    const VkDeviceSize Bytes =
        static_cast<VkDeviceSize>(InFlightSize.width) * InFlightSize.height * 4;
    const void* Pixels = Staging.Map(Bytes);
    LastSucceeded =
        !Error &&
        stbi_write_png(InFlightPath.c_str(), static_cast<int>(InFlightSize.width),
                       static_cast<int>(InFlightSize.height), 4, Pixels,
                       static_cast<int>(InFlightSize.width) * 4) != 0;
    Staging.Unmap();
    Staging.Destroy();

    LastResult = LastSucceeded ? fs::absolute(Target, Error).string()
                               : "Could not write " + InFlightPath;
    InFlightPath.clear();
    SaveCount++;
}

void Screenshot::Destroy()
{
    Staging.Destroy();
    InFlightPath.clear();
    RequestedPath.clear();
}

}  // namespace rtrt
