#ifndef RTRT_SRC_BUFFER_WRAP_H_
#define RTRT_SRC_BUFFER_WRAP_H_

#include <vulkan/vulkan_core.h>

struct BufferWrap
{
    VkBuffer buffer;
    VkDeviceMemory memory;

    void destroy(VkDevice& device)
    {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }
};

#endif  // RTRT_SRC_BUFFER_WRAP_H_
