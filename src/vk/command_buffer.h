#ifndef RTRT_SRC_VK_COMMAND_BUFFER_H_
#define RTRT_SRC_VK_COMMAND_BUFFER_H_

#include <vulkan/vulkan_core.h>

#include "vk/command_pool.h"

namespace rtrt
{

// A primary command buffer that is recorded again every frame. The pool it came from
// frees it.
class CommandBuffer
{
public:
    void Allocate(const CommandPool& Pool)
    {
        Handle = Pool.Allocate();
    }

    // Starts recording for one submit.
    void Begin();
    void End();

    VkCommandBuffer GetHandle() const
    {
        return Handle;
    }

private:
    VkCommandBuffer Handle = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_COMMAND_BUFFER_H_
