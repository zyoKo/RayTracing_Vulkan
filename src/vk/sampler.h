#ifndef RTRT_SRC_VK_SAMPLER_H_
#define RTRT_SRC_VK_SAMPLER_H_

#include <vulkan/vulkan_core.h>

#include "vk/device.h"

namespace rtrt
{

// A linear, repeating sampler with the GPU's highest anisotropy. Frees itself when
// destroyed; moves hand the handle over.
class Sampler
{
public:
    Sampler() = default;
    ~Sampler();
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;
    Sampler(Sampler&& Other) noexcept;
    Sampler& operator=(Sampler&& Other) noexcept;

    void Create(const Device& Gpu);

    // Safe to call on an empty sampler.
    void Destroy();

    VkSampler GetHandle() const
    {
        return Handle;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkSampler Handle = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_SAMPLER_H_
