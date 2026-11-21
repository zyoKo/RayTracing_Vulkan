#include "vk/framebuffer.h"

#include <utility>
#include <vector>

#include "vk_check.h"

namespace rtrt
{

Framebuffer::~Framebuffer()
{
    Destroy();
}

Framebuffer::Framebuffer(Framebuffer&& Other) noexcept
    : DeviceHandle(std::exchange(Other.DeviceHandle, VK_NULL_HANDLE)),
      Handle(std::exchange(Other.Handle, VK_NULL_HANDLE))
{
}

Framebuffer& Framebuffer::operator=(Framebuffer&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        DeviceHandle = std::exchange(Other.DeviceHandle, VK_NULL_HANDLE);
        Handle = std::exchange(Other.Handle, VK_NULL_HANDLE);
    }
    return *this;
}

void Framebuffer::Create(VkDevice LogicalDevice, VkRenderPass Pass,
                         const std::vector<VkImageView>& Attachments, VkExtent2D Size)
{
    Destroy();
    DeviceHandle = LogicalDevice;

    VkFramebufferCreateInfo Info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    Info.renderPass = Pass;
    Info.attachmentCount = static_cast<uint32_t>(Attachments.size());
    Info.pAttachments = Attachments.data();
    Info.width = Size.width;
    Info.height = Size.height;
    Info.layers = 1;
    RTRT_CHECK_VK(vkCreateFramebuffer(DeviceHandle, &Info, nullptr, &Handle));
}

void Framebuffer::Destroy()
{
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyFramebuffer(DeviceHandle, Handle, nullptr);
    Handle = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
}

}  // namespace rtrt
