#include "renderer/render_image.h"

namespace rtrt
{

Image CreateRenderImage(const Device& Gpu, VkExtent2D Size)
{
    Image Result;
    Result.Create(Gpu, Size.width, Size.height, VK_FORMAT_R32G32B32A32_SFLOAT,
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Result.CreateView(VK_FORMAT_R32G32B32A32_SFLOAT);
    Result.CreateSampler(Gpu);
    Result.SetLayout(VK_IMAGE_LAYOUT_GENERAL);
    return Result;
}

}  // namespace rtrt
