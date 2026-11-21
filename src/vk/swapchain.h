#ifndef RTRT_SRC_VK_SWAPCHAIN_H_
#define RTRT_SRC_VK_SWAPCHAIN_H_

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "core/window.h"
#include "vk/device.h"
#include "vk/semaphore.h"

namespace rtrt
{

// The window's swapchain: its images and views, the semaphore an acquire signals, and
// one semaphore per image that a present waits on.
class Swapchain
{
public:
    Swapchain() = default;
    ~Swapchain();
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // Creates the swapchain for Surface at the window's size. Call Destroy first to
    // rebuild it.
    void Create(const Device& Gpu, VkSurfaceKHR Surface, const Window& Target);
    void Destroy();

    // Records the move of every image to the present layout.
    void RecordInitialTransition(VkCommandBuffer Cmd) const;

    // Picks the next image and signals GetAcquireSemaphore when it's free. Returns
    // the vkAcquireNextImageKHR result.
    VkResult AcquireNextImage();

    // Presents the acquired image once GetRenderedSemaphore is signalled.
    VkResult Present(VkQueue Queue);

    VkExtent2D GetExtent() const
    {
        return Extent;
    }

    uint32_t GetImageCount() const
    {
        return static_cast<uint32_t>(Images.size());
    }

    VkImageView GetView(uint32_t Index) const
    {
        return Views[Index];
    }

    uint32_t GetImageIndex() const
    {
        return ImageIndex;
    }

    VkSemaphore GetAcquireSemaphore() const
    {
        return AcquireSemaphore.GetHandle();
    }

    // The semaphore for the acquired image.
    VkSemaphore GetRenderedSemaphore() const
    {
        return RenderedSemaphores[ImageIndex].GetHandle();
    }

private:
    static VkSurfaceFormatKHR ChooseSurfaceFormat(VkPhysicalDevice Gpu,
                                                  VkSurfaceKHR Surface);
    static uint32_t ChooseImageCount(const VkSurfaceCapabilitiesKHR& Capabilities);
    static VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& Capabilities,
                                   const Window& Target);
    void CreateViews(VkFormat Format);
    void CreateSemaphores();

    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkSwapchainKHR Handle = VK_NULL_HANDLE;
    std::vector<VkImage> Images;
    std::vector<VkImageView> Views;
    Semaphore AcquireSemaphore;
    // A present may still wait on an image's semaphore, so each image gets its own.
    std::vector<Semaphore> RenderedSemaphores;
    VkExtent2D Extent{0, 0};
    uint32_t ImageIndex = 0;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_SWAPCHAIN_H_
