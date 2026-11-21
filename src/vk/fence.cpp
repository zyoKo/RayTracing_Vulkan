#include "vk/fence.h"

#include "vk_check.h"

namespace rtrt
{

Fence::~Fence()
{
    Destroy();
}

void Fence::Create(const Device& Gpu, bool Signaled)
{
    Destroy();
    DeviceHandle = Gpu.GetHandle();

    VkFenceCreateInfo FenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    FenceCreateInfo.flags = Signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    RTRT_CHECK_VK(vkCreateFence(DeviceHandle, &FenceCreateInfo, nullptr, &Handle));
}

void Fence::Destroy()
{
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyFence(DeviceHandle, Handle, nullptr);
    Handle = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
}

void Fence::Wait() const
{
    // Wait in 1 ms slices so a timeout never reaches CheckVkResult.
    VkResult Result = VK_TIMEOUT;
    while (Result == VK_TIMEOUT)
    {
        Result = vkWaitForFences(DeviceHandle, 1, &Handle, VK_TRUE, 1'000'000);
    }
    CheckVkResult(Result, "vkWaitForFences", __FILE__, __LINE__);
}

void Fence::Reset() const
{
    RTRT_CHECK_VK(vkResetFences(DeviceHandle, 1, &Handle));
}

}  // namespace rtrt
