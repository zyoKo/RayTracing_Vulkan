#ifndef RTRT_SRC_VK_FENCE_H_
#define RTRT_SRC_VK_FENCE_H_

#include <vulkan/vulkan_core.h>

#include "vk/device.h"

namespace rtrt
{

// A fence the CPU waits on for submitted work. Frees itself when destroyed.
class Fence
{
public:
    Fence() = default;
    ~Fence();
    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;

    // Signaled starts the fence as already signalled, so the first Wait returns at once.
    void Create(const Device& Gpu, bool Signaled);

    // Safe to call on an empty fence.
    void Destroy();

    // Blocks until the fence is signalled.
    void Wait() const;
    void Reset() const;

    VkFence GetHandle() const
    {
        return Handle;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkFence Handle = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_FENCE_H_
