#include "scene/texture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#include "vk/buffer.h"
#include "vk_check.h"

namespace rtrt
{

Image LoadTexture(const Device& Gpu, const CommandPool& Pool, const std::string& FileName)
{
    int TexWidth, TexHeight, TexChannels;

    stbi_set_flip_vertically_on_load(true);
    stbi_uc* Pixels =
        stbi_load(FileName.c_str(), &TexWidth, &TexHeight, &TexChannels, STBI_rgb_alpha);
    if (!Pixels)
    {
        RTRT_FATAL("Failed to load texture %s: %s", FileName.c_str(),
                   stbi_failure_reason());
    }
    const VkDeviceSize ImageSize = static_cast<VkDeviceSize>(TexWidth) * TexHeight * 4;

    Buffer Staging;
    Staging.Create(
        Gpu, ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    memcpy(Staging.Map(ImageSize), Pixels, static_cast<size_t>(ImageSize));
    Staging.Unmap();
    stbi_image_free(Pixels);

    const uint32_t MipLevels =
        static_cast<uint32_t>(std::floor(std::log2(std::max(TexWidth, TexHeight)))) + 1;

    Image Texture;
    Texture.Create(Gpu, TexWidth, TexHeight, VK_FORMAT_R8G8B8A8_UNORM,
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MipLevels);
    Texture.TransitionLayout(Pool, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, MipLevels);
    Texture.CopyFrom(Pool, Staging.GetHandle(), static_cast<uint32_t>(TexWidth),
                     static_cast<uint32_t>(TexHeight));
    Staging.Destroy();
    Texture.GenerateMipmaps(Gpu, Pool, VK_FORMAT_R8G8B8A8_UNORM, TexWidth, TexHeight,
                            MipLevels);

    Texture.CreateView(VK_FORMAT_R8G8B8A8_UNORM);
    Texture.CreateSampler(Gpu);
    Texture.SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return Texture;
}

}  // namespace rtrt
