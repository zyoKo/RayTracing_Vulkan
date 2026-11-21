#include "vk/command_pool.h"

#include "vk_check.h"

namespace rtrt
{

CommandPool::~CommandPool()
{
    Destroy();
}

void CommandPool::Create(const Device& Gpu)
{
    Destroy();
    DeviceHandle = Gpu.GetHandle();
    Queue = Gpu.GetQueue();

    VkCommandPoolCreateInfo PoolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    PoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    PoolCreateInfo.queueFamilyIndex = Gpu.GetQueueFamily();
    RTRT_CHECK_VK(vkCreateCommandPool(DeviceHandle, &PoolCreateInfo, nullptr, &Handle));
}

void CommandPool::Destroy()
{
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyCommandPool(DeviceHandle, Handle, nullptr);
    Handle = VK_NULL_HANDLE;
    Queue = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
}

VkCommandBuffer CommandPool::Allocate() const
{
    VkCommandBufferAllocateInfo AllocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    AllocateInfo.commandPool = Handle;
    AllocateInfo.commandBufferCount = 1;
    AllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer Cmd;
    RTRT_CHECK_VK(vkAllocateCommandBuffers(DeviceHandle, &AllocateInfo, &Cmd));
    return Cmd;
}

VkCommandBuffer CommandPool::BeginSingleUse() const
{
    VkCommandBuffer Cmd = Allocate();
    VkCommandBufferBeginInfo BeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    RTRT_CHECK_VK(vkBeginCommandBuffer(Cmd, &BeginInfo));
    return Cmd;
}

void CommandPool::EndSingleUse(VkCommandBuffer Cmd) const
{
    RTRT_CHECK_VK(vkEndCommandBuffer(Cmd));

    VkSubmitInfo SubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &Cmd;
    RTRT_CHECK_VK(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));
    RTRT_CHECK_VK(vkQueueWaitIdle(Queue));
    vkFreeCommandBuffers(DeviceHandle, Handle, 1, &Cmd);
}

}  // namespace rtrt
