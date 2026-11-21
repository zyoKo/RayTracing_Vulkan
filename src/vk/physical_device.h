#ifndef RTRT_SRC_VK_PHYSICAL_DEVICE_H_
#define RTRT_SRC_VK_PHYSICAL_DEVICE_H_

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace rtrt
{

// The GPU the renderer runs on: the first one that supports every required device
// extension. Owns nothing; the instance owns the handle.
class PhysicalDevice
{
public:
    // Picks the GPU. False when no GPU supports all of RequiredExtensions.
    bool Init(VkInstance Instance, const std::vector<const char*>& RequiredExtensions);

    // The first queue family with graphics, compute and transfer, or false if none.
    bool FindQueueFamily(uint32_t& Family) const;

    // Index of a memory type allowed by TypeFilter that has all of Properties.
    uint32_t FindMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags Properties) const;

    VkPhysicalDevice GetHandle() const
    {
        return Handle;
    }

private:
    static bool SupportsExtensions(VkPhysicalDevice Candidate,
                                   const std::vector<const char*>& RequiredExtensions);

    VkPhysicalDevice Handle = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_PHYSICAL_DEVICE_H_
