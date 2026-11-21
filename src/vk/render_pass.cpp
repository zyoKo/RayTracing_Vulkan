#include "vk/render_pass.h"

#include "vk_check.h"

namespace rtrt
{

RenderPass::~RenderPass()
{
    Destroy();
}

void RenderPass::Create(VkDevice LogicalDevice, const VkRenderPassCreateInfo& Info)
{
    Destroy();
    DeviceHandle = LogicalDevice;
    RTRT_CHECK_VK(vkCreateRenderPass(DeviceHandle, &Info, nullptr, &Handle));
}

void RenderPass::Destroy()
{
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyRenderPass(DeviceHandle, Handle, nullptr);
    Handle = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
}

}  // namespace rtrt
