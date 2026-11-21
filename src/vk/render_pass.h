#ifndef RTRT_SRC_VK_RENDER_PASS_H_
#define RTRT_SRC_VK_RENDER_PASS_H_

#include <vulkan/vulkan_core.h>

namespace rtrt
{

// A render pass. Frees itself when destroyed.
class RenderPass
{
public:
    RenderPass() = default;
    ~RenderPass();
    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    // Creates the render pass from a filled-in create info.
    void Create(VkDevice LogicalDevice, const VkRenderPassCreateInfo& Info);

    // Safe to call on an empty render pass.
    void Destroy();

    VkRenderPass GetHandle() const
    {
        return Handle;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkRenderPass Handle = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_RENDER_PASS_H_
