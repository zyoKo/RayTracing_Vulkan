#ifndef RTRT_SRC_BUFFER_WRAP_H_
#define RTRT_SRC_BUFFER_WRAP_H_

#include <vulkan/vulkan_core.h>

namespace rtrt
{

struct BufferWrap
{
    VkBuffer Buffer;
    VkDeviceMemory Memory;

    void Destroy(VkDevice Device)
    {
        vkDestroyBuffer(Device, Buffer, nullptr);
        vkFreeMemory(Device, Memory, nullptr);
    }
};

}  // namespace rtrt

#endif  // RTRT_SRC_BUFFER_WRAP_H_
