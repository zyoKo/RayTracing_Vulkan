#ifndef RTRT_SRC_VK_COMMAND_POOL_H_
#define RTRT_SRC_VK_COMMAND_POOL_H_

#include <vulkan/vulkan_core.h>

#include "vk/device.h"

namespace rtrt
{

// A command pool on the device's queue family whose buffers can be reset one by one.
// Also runs single-use command buffers to completion.
class CommandPool
{
public:
    CommandPool() = default;
    ~CommandPool();
    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;

    void Create(const Device& Gpu);

    // Frees the pool and every buffer from it. Safe to call on an empty pool.
    void Destroy();

    // A new primary command buffer, owned by the pool.
    VkCommandBuffer Allocate() const;

    // A new command buffer, already begun for one submit.
    VkCommandBuffer BeginSingleUse() const;

    // Ends Cmd, submits it, waits for the queue to go idle and frees it.
    void EndSingleUse(VkCommandBuffer Cmd) const;

    VkCommandPool GetHandle() const
    {
        return Handle;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkQueue Queue = VK_NULL_HANDLE;
    VkCommandPool Handle = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_COMMAND_POOL_H_
