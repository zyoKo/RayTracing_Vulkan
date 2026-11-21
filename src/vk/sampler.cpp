#include "vk/sampler.h"

#include <utility>

#include "vk_check.h"

namespace rtrt
{

Sampler::~Sampler()
{
    Destroy();
}

Sampler::Sampler(Sampler&& Other) noexcept
    : DeviceHandle(std::exchange(Other.DeviceHandle, VK_NULL_HANDLE)),
      Handle(std::exchange(Other.Handle, VK_NULL_HANDLE))
{
}

Sampler& Sampler::operator=(Sampler&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        DeviceHandle = std::exchange(Other.DeviceHandle, VK_NULL_HANDLE);
        Handle = std::exchange(Other.Handle, VK_NULL_HANDLE);
    }
    return *this;
}

void Sampler::Create(const Device& Gpu)
{
    Destroy();
    DeviceHandle = Gpu.GetHandle();

    VkPhysicalDeviceProperties Properties{};
    vkGetPhysicalDeviceProperties(Gpu.GetPhysical(), &Properties);

    VkSamplerCreateInfo SamplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    SamplerInfo.magFilter = VK_FILTER_LINEAR;
    SamplerInfo.minFilter = VK_FILTER_LINEAR;
    SamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    SamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    SamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    SamplerInfo.anisotropyEnable = VK_TRUE;
    SamplerInfo.maxAnisotropy = Properties.limits.maxSamplerAnisotropy;
    SamplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    SamplerInfo.unnormalizedCoordinates = VK_FALSE;
    SamplerInfo.compareEnable = VK_FALSE;
    SamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    RTRT_CHECK_VK(vkCreateSampler(DeviceHandle, &SamplerInfo, nullptr, &Handle));
}

void Sampler::Destroy()
{
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroySampler(DeviceHandle, Handle, nullptr);
    Handle = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
}

}  // namespace rtrt
