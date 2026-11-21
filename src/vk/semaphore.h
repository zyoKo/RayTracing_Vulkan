#ifndef RTRT_SRC_VK_SEMAPHORE_H_
#define RTRT_SRC_VK_SEMAPHORE_H_

#include <vulkan/vulkan_core.h>

namespace rtrt
{

// A binary semaphore that orders work on the GPU. Frees itself when destroyed; moves
// hand the handle over.
class Semaphore
{
public:
    Semaphore() = default;
    ~Semaphore();
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;
    Semaphore(Semaphore&& Other) noexcept;
    Semaphore& operator=(Semaphore&& Other) noexcept;

    void Create(VkDevice LogicalDevice);

    // Safe to call on an empty semaphore.
    void Destroy();

    VkSemaphore GetHandle() const
    {
        return Handle;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkSemaphore Handle = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_SEMAPHORE_H_
