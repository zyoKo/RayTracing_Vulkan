#include "vk/device.h"

#include <cstdio>

#include "extensions_vk.hpp"
#include "vk_check.h"

namespace rtrt
{

Device::~Device()
{
    if (Handle != VK_NULL_HANDLE)
    {
        vkDestroyDevice(Handle, nullptr);
    }
}

bool Device::Init(const Context& Instance)
{
    if (!Gpu.Init(Instance.GetInstance(), ReqDeviceExtensions))
    {
        fprintf(stderr, "No GPU supports the required ray tracing extensions.\n");
        return false;
    }
    if (!Gpu.FindQueueFamily(QueueFamily))
    {
        fprintf(stderr, "No queue family supports graphics, compute and transfer.\n");
        return false;
    }

    CreateLogicalDevice();
    vkGetDeviceQueue(Handle, QueueFamily, 0, &Queue);

    // Function pointers for every extension, from the generated extensions_vk.cpp
    load_VK_EXTENSIONS(Instance.GetInstance(), vkGetInstanceProcAddr, Handle,
                       vkGetDeviceProcAddr);

    CheckPresentSupport(Instance.GetSurface());
    return true;
}

void Device::CreateLogicalDevice()
{
    // pNext chain of feature structures:
    //   features2->features11->features12->features13->accelFeature->rtPipelineFeature->NULL
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR RtPipelineFeature{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, nullptr};

    VkPhysicalDeviceAccelerationStructureFeaturesKHR AccelFeature{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        &RtPipelineFeature};

    VkPhysicalDeviceVulkan13Features Features13{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, &AccelFeature};

    VkPhysicalDeviceVulkan12Features Features12{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &Features13};

    VkPhysicalDeviceVulkan11Features Features11{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, &Features12};

    VkPhysicalDeviceFeatures2 Features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                        &Features11};

    // Fill in all structures on the pNext chain
    vkGetPhysicalDeviceFeatures2(Gpu.GetHandle(), &Features2);

    // Bounds-checked buffer access costs performance and isn't needed here
    Features2.features.robustBufferAccess = VK_FALSE;

    float Priority = 1.0;
    VkDeviceQueueCreateInfo QueueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    QueueInfo.queueFamilyIndex = QueueFamily;
    QueueInfo.queueCount = 1;
    QueueInfo.pQueuePriorities = &Priority;

    VkDeviceCreateInfo DeviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    DeviceCreateInfo.pNext = &Features2;  // This is the whole pNext chain

    DeviceCreateInfo.queueCreateInfoCount = 1;
    DeviceCreateInfo.pQueueCreateInfos = &QueueInfo;

    DeviceCreateInfo.enabledExtensionCount =
        static_cast<uint32_t>(ReqDeviceExtensions.size());
    DeviceCreateInfo.ppEnabledExtensionNames = ReqDeviceExtensions.data();

    RTRT_CHECK_VK(vkCreateDevice(Gpu.GetHandle(), &DeviceCreateInfo, nullptr, &Handle));
}

void Device::CheckPresentSupport(VkSurfaceKHR Surface) const
{
    VkBool32 IsSupported;
    RTRT_CHECK_VK(vkGetPhysicalDeviceSurfaceSupportKHR(Gpu.GetHandle(), QueueFamily,
                                                       Surface, &IsSupported));

    if (IsSupported != VK_TRUE)
    {
        printf("\nVulkan does not support presenting on this surface!\n");
    }
}

}  // namespace rtrt
