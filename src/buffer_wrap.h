
# pragma once

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
