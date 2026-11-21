#include "vk/buffer.h"

#include <cstring>
#include <utility>

#include "vk_check.h"

namespace rtrt
{

Buffer::~Buffer()
{
    Destroy();
}

Buffer::Buffer(Buffer&& Other) noexcept
    : DeviceHandle(std::exchange(Other.DeviceHandle, VK_NULL_HANDLE)),
      Handle(std::exchange(Other.Handle, VK_NULL_HANDLE)),
      Memory(std::exchange(Other.Memory, VK_NULL_HANDLE))
{
}

Buffer& Buffer::operator=(Buffer&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        DeviceHandle = std::exchange(Other.DeviceHandle, VK_NULL_HANDLE);
        Handle = std::exchange(Other.Handle, VK_NULL_HANDLE);
        Memory = std::exchange(Other.Memory, VK_NULL_HANDLE);
    }
    return *this;
}

void Buffer::Create(const Device& Gpu, VkDeviceSize Size, VkBufferUsageFlags Usage,
                    VkMemoryPropertyFlags Properties)
{
    Destroy();
    DeviceHandle = Gpu.GetHandle();

    VkBufferCreateInfo BufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    BufferInfo.size = Size;
    BufferInfo.usage = Usage;
    BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    RTRT_CHECK_VK(vkCreateBuffer(DeviceHandle, &BufferInfo, nullptr, &Handle));

    VkMemoryRequirements MemRequirements;
    vkGetBufferMemoryRequirements(DeviceHandle, Handle, &MemRequirements);

    VkMemoryAllocateFlagsInfo MemFlags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
                                          nullptr, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
                                          0};

    VkMemoryAllocateInfo AllocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    AllocInfo.pNext = &MemFlags;
    AllocInfo.allocationSize = MemRequirements.size;
    AllocInfo.memoryTypeIndex =
        Gpu.FindMemoryType(MemRequirements.memoryTypeBits, Properties);
    RTRT_CHECK_VK(vkAllocateMemory(DeviceHandle, &AllocInfo, nullptr, &Memory));

    RTRT_CHECK_VK(vkBindBufferMemory(DeviceHandle, Handle, Memory, 0));
}

void Buffer::CreateWithData(const Device& Gpu, const CommandPool& Pool, VkDeviceSize Size,
                            const void* Data, VkBufferUsageFlags Usage)
{
    Buffer Staging;
    Staging.Create(
        Gpu, Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    memcpy(Staging.Map(Size), Data, Size);
    Staging.Unmap();

    Create(Gpu, Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | Usage,
           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandBuffer Cmd = Pool.BeginSingleUse();
    VkBufferCopy CopyRegion{};
    CopyRegion.size = Size;
    vkCmdCopyBuffer(Cmd, Staging.GetHandle(), Handle, 1, &CopyRegion);
    Pool.EndSingleUse(Cmd);
}

void Buffer::Destroy()
{
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyBuffer(DeviceHandle, Handle, nullptr);
    vkFreeMemory(DeviceHandle, Memory, nullptr);
    Handle = VK_NULL_HANDLE;
    Memory = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
}

void* Buffer::Map(VkDeviceSize Size) const
{
    void* Data = nullptr;
    RTRT_CHECK_VK(vkMapMemory(DeviceHandle, Memory, 0, Size, 0, &Data));
    return Data;
}

void Buffer::Unmap() const
{
    vkUnmapMemory(DeviceHandle, Memory);
}

VkDeviceAddress Buffer::GetAddress() const
{
    VkBufferDeviceAddressInfo Info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    Info.buffer = Handle;
    return vkGetBufferDeviceAddress(DeviceHandle, &Info);
}

}  // namespace rtrt
