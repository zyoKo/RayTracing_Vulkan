#include "vk/swapchain.h"

#include <assert.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "vk_check.h"

namespace rtrt
{

Swapchain::~Swapchain()
{
    Destroy();
}

void Swapchain::Create(const Device& Gpu, VkSurfaceKHR Surface, const Window& Target)
{
    DeviceHandle = Gpu.GetHandle();

    VkSurfaceCapabilitiesKHR Capabilities;
    RTRT_CHECK_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Gpu.GetPhysical(), Surface,
                                                            &Capabilities));

    const VkSurfaceFormatKHR Format = ChooseSurfaceFormat(Gpu.GetPhysical(), Surface);

    Extent = ChooseExtent(Capabilities, Target);
    // A zero size typically means the window is minimized, which isn't handled yet.
    assert(Extent.width && Extent.height);

    const uint32_t ImageCount = ChooseImageCount(Capabilities);
    // Other code assumes three swapchain images
    assert(ImageCount == 3);

    const uint32_t QueueFamily = Gpu.GetQueueFamily();

    VkSwapchainCreateInfoKHR SwapchainInfo = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    SwapchainInfo.surface = Surface;
    SwapchainInfo.minImageCount = ImageCount;
    SwapchainInfo.imageFormat = Format.format;
    SwapchainInfo.imageColorSpace = Format.colorSpace;
    SwapchainInfo.imageExtent = Extent;
    SwapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_STORAGE_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    SwapchainInfo.preTransform = Capabilities.currentTransform;
    SwapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    SwapchainInfo.imageArrayLayers = 1;
    SwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SwapchainInfo.queueFamilyIndexCount = 1;
    SwapchainInfo.pQueueFamilyIndices = &QueueFamily;
    // FIFO is the one present mode every implementation must support.
    SwapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    SwapchainInfo.oldSwapchain = VK_NULL_HANDLE;
    SwapchainInfo.clipped = true;

    RTRT_CHECK_VK(vkCreateSwapchainKHR(DeviceHandle, &SwapchainInfo, nullptr, &Handle));

    uint32_t Count = 0;
    RTRT_CHECK_VK(vkGetSwapchainImagesKHR(DeviceHandle, Handle, &Count, nullptr));
    Images.resize(Count);
    RTRT_CHECK_VK(vkGetSwapchainImagesKHR(DeviceHandle, Handle, &Count, Images.data()));

    CreateViews(Format.format);
    CreateSemaphores();
}

void Swapchain::Destroy()
{
    if (Handle == VK_NULL_HANDLE)
    {
        return;
    }
    RTRT_CHECK_VK(vkDeviceWaitIdle(DeviceHandle));

    for (VkImageView View : Views)
    {
        vkDestroyImageView(DeviceHandle, View, nullptr);
    }
    vkDestroySwapchainKHR(DeviceHandle, Handle, nullptr);

    Handle = VK_NULL_HANDLE;
    AcquireSemaphore.Destroy();
    Images.clear();
    Views.clear();
    RenderedSemaphores.clear();
}

void Swapchain::RecordInitialTransition(VkCommandBuffer Cmd) const
{
    std::vector<VkImageMemoryBarrier> Barriers(Images.size());
    for (size_t I = 0; I < Images.size(); I++)
    {
        VkImageMemoryBarrier& Barrier = Barriers[I];
        Barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        Barrier.srcAccessMask = 0;
        Barrier.dstAccessMask = 0;
        Barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        Barrier.image = Images[I];
        Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Barrier.subresourceRange.baseMipLevel = 0;
        Barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        Barrier.subresourceRange.baseArrayLayer = 0;
        Barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    }

    vkCmdPipelineBarrier(Cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                         static_cast<uint32_t>(Barriers.size()), Barriers.data());
}

VkResult Swapchain::AcquireNextImage()
{
    return vkAcquireNextImageKHR(DeviceHandle, Handle, UINT64_MAX,
                                 AcquireSemaphore.GetHandle(), VK_NULL_HANDLE,
                                 &ImageIndex);
}

VkResult Swapchain::Present(VkQueue Queue)
{
    const VkSemaphore Rendered = GetRenderedSemaphore();

    VkPresentInfoKHR PresentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = &Rendered;
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = &Handle;
    PresentInfo.pImageIndices = &ImageIndex;
    return vkQueuePresentKHR(Queue, &PresentInfo);
}

VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat(VkPhysicalDevice Gpu,
                                                  VkSurfaceKHR Surface)
{
    uint32_t Count;
    RTRT_CHECK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(Gpu, Surface, &Count, nullptr));
    std::vector<VkSurfaceFormatKHR> Formats(Count);
    RTRT_CHECK_VK(
        vkGetPhysicalDeviceSurfaceFormatsKHR(Gpu, Surface, &Count, Formats.data()));
    return Formats[0];
}

uint32_t Swapchain::ChooseImageCount(const VkSurfaceCapabilitiesKHR& Capabilities)
{
    // One more than the minimum, within the supported range
    uint32_t ImageCount = Capabilities.minImageCount + 1;
    if (Capabilities.maxImageCount > 0 && ImageCount > Capabilities.maxImageCount)
    {
        ImageCount = Capabilities.maxImageCount;
    }
    return ImageCount;
}

VkExtent2D Swapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& Capabilities,
                                   const Window& Target)
{
    if (Capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return Capabilities.currentExtent;
    }

    VkExtent2D Size = Target.GetFramebufferSize();
    Size.width = std::clamp(Size.width, Capabilities.minImageExtent.width,
                            Capabilities.maxImageExtent.width);
    Size.height = std::clamp(Size.height, Capabilities.minImageExtent.height,
                             Capabilities.maxImageExtent.height);
    return Size;
}

void Swapchain::CreateViews(VkFormat Format)
{
    Views.resize(Images.size());
    for (size_t I = 0; I < Images.size(); I++)
    {
        VkImageViewCreateInfo CreateInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        CreateInfo.image = Images[I];
        CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        CreateInfo.format = Format;
        CreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        CreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        CreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        CreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        CreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        CreateInfo.subresourceRange.baseMipLevel = 0;
        CreateInfo.subresourceRange.levelCount = 1;
        CreateInfo.subresourceRange.baseArrayLayer = 0;
        CreateInfo.subresourceRange.layerCount = 1;
        RTRT_CHECK_VK(vkCreateImageView(DeviceHandle, &CreateInfo, nullptr, &Views[I]));
    }
}

void Swapchain::CreateSemaphores()
{
    AcquireSemaphore.Create(DeviceHandle);
    RenderedSemaphores.resize(Images.size());
    for (Semaphore& Sem : RenderedSemaphores)
    {
        Sem.Create(DeviceHandle);
    }
}

}  // namespace rtrt
