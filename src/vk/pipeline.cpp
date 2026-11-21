#include "vk/pipeline.h"

#include <utility>

#include "vk_check.h"

namespace rtrt
{

Pipeline::~Pipeline()
{
    Destroy();
}

Pipeline::Pipeline(Pipeline&& Other) noexcept
    : DeviceHandle(std::exchange(Other.DeviceHandle, VK_NULL_HANDLE)),
      Handle(std::exchange(Other.Handle, VK_NULL_HANDLE))
{
}

Pipeline& Pipeline::operator=(Pipeline&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        DeviceHandle = std::exchange(Other.DeviceHandle, VK_NULL_HANDLE);
        Handle = std::exchange(Other.Handle, VK_NULL_HANDLE);
    }
    return *this;
}

void Pipeline::CreateGraphics(VkDevice LogicalDevice,
                              const VkGraphicsPipelineCreateInfo& Info)
{
    Destroy();
    DeviceHandle = LogicalDevice;
    RTRT_CHECK_VK(vkCreateGraphicsPipelines(DeviceHandle, VK_NULL_HANDLE, 1, &Info,
                                            nullptr, &Handle));
}

void Pipeline::CreateCompute(VkDevice LogicalDevice,
                             const VkComputePipelineCreateInfo& Info)
{
    Destroy();
    DeviceHandle = LogicalDevice;
    RTRT_CHECK_VK(vkCreateComputePipelines(DeviceHandle, VK_NULL_HANDLE, 1, &Info,
                                           nullptr, &Handle));
}

void Pipeline::CreateRayTracing(VkDevice LogicalDevice,
                                const VkRayTracingPipelineCreateInfoKHR& Info)
{
    Destroy();
    DeviceHandle = LogicalDevice;
    RTRT_CHECK_VK(vkCreateRayTracingPipelinesKHR(
        DeviceHandle, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &Info, nullptr, &Handle));
}

void Pipeline::Destroy()
{
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyPipeline(DeviceHandle, Handle, nullptr);
    Handle = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
}

}  // namespace rtrt
