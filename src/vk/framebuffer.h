#ifndef RTRT_SRC_VK_FRAMEBUFFER_H_
#define RTRT_SRC_VK_FRAMEBUFFER_H_

#include <vector>

#include <vulkan/vulkan_core.h>

namespace rtrt
{

// A framebuffer: the image views one render pass draws into. Frees itself when
// destroyed; moves hand the handle over.
class Framebuffer
{
public:
    Framebuffer() = default;
    ~Framebuffer();
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& Other) noexcept;
    Framebuffer& operator=(Framebuffer&& Other) noexcept;

    // A one-layer framebuffer of Size for Pass, with Attachments in the pass's order.
    void Create(VkDevice LogicalDevice, VkRenderPass Pass,
                const std::vector<VkImageView>& Attachments, VkExtent2D Size);

    // Safe to call on an empty framebuffer.
    void Destroy();

    VkFramebuffer GetHandle() const
    {
        return Handle;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkFramebuffer Handle = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_FRAMEBUFFER_H_
