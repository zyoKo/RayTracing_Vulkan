#ifndef RTRT_SRC_VK_BUFFER_H_
#define RTRT_SRC_VK_BUFFER_H_

#include <vulkan/vulkan_core.h>

#include "vk/device.h"

namespace rtrt
{

// A VkBuffer and the memory bound to it, allocated with device addresses on. Frees both
// when destroyed; moves hand the handles over.
class Buffer
{
public:
    Buffer() = default;
    ~Buffer();
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& Other) noexcept;
    Buffer& operator=(Buffer&& Other) noexcept;

    // Creates the buffer and binds memory that has all of Properties.
    void Create(const Device& Gpu, VkDeviceSize Size, VkBufferUsageFlags Usage,
                VkMemoryPropertyFlags Properties);

    // Frees the buffer and its memory. Safe to call on an empty buffer.
    void Destroy();

    // Maps Size bytes of host-visible memory.
    void* Map(VkDeviceSize Size) const;
    void Unmap() const;

    VkDeviceAddress GetAddress() const;

    VkBuffer GetHandle() const
    {
        return Handle;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkBuffer Handle = VK_NULL_HANDLE;
    VkDeviceMemory Memory = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_BUFFER_H_
