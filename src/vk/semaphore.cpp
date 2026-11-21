#include "vk/semaphore.h"

#include <utility>

#include "vk_check.h"

namespace rtrt
{

Semaphore::~Semaphore()
{
    Destroy();
}

Semaphore::Semaphore(Semaphore&& Other) noexcept
    : DeviceHandle(std::exchange(Other.DeviceHandle, VK_NULL_HANDLE)),
      Handle(std::exchange(Other.Handle, VK_NULL_HANDLE))
{
}

Semaphore& Semaphore::operator=(Semaphore&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        DeviceHandle = std::exchange(Other.DeviceHandle, VK_NULL_HANDLE);
        Handle = std::exchange(Other.Handle, VK_NULL_HANDLE);
    }
    return *this;
}

void Semaphore::Create(VkDevice LogicalDevice)
{
    Destroy();
    DeviceHandle = LogicalDevice;
    VkSemaphoreCreateInfo CreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    RTRT_CHECK_VK(vkCreateSemaphore(DeviceHandle, &CreateInfo, nullptr, &Handle));
}

void Semaphore::Destroy()
{
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroySemaphore(DeviceHandle, Handle, nullptr);
    Handle = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
}

}  // namespace rtrt
