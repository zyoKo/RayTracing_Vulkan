#include "vk/physical_device.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "vk_check.h"

namespace rtrt
{

bool PhysicalDevice::Init(VkInstance Instance,
                          const std::vector<const char*>& RequiredExtensions)
{
    uint32_t PhysicalDevicesCount;
    RTRT_CHECK_VK(vkEnumeratePhysicalDevices(Instance, &PhysicalDevicesCount, nullptr));
    std::vector<VkPhysicalDevice> PhysicalDevices(PhysicalDevicesCount);
    RTRT_CHECK_VK(vkEnumeratePhysicalDevices(Instance, &PhysicalDevicesCount,
                                             PhysicalDevices.data()));

    printf("%d devices\n", PhysicalDevicesCount);

    for (auto& Candidate : PhysicalDevices)
    {
        VkPhysicalDeviceProperties GPUproperties;
        vkGetPhysicalDeviceProperties(Candidate, &GPUproperties);

        printf("\nGPU: %s", GPUproperties.deviceName);
        if (GPUproperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            printf(" supports ");
        }
        else
        {
            printf(" does not supports ");
        }
        printf("VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU properties!\n");

        if (SupportsExtensions(Candidate, RequiredExtensions))
        {
            Handle = Candidate;
            return true;
        }
    }
    return false;
}

bool PhysicalDevice::FindQueueFamily(uint32_t& Family) const
{
    const VkQueueFlags RequiredFlags =
        VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

    uint32_t Count;
    vkGetPhysicalDeviceQueueFamilyProperties(Handle, &Count, nullptr);
    std::vector<VkQueueFamilyProperties> Properties(Count);
    vkGetPhysicalDeviceQueueFamilyProperties(Handle, &Count, Properties.data());

    for (uint32_t I = 0; I < Count; I++)
    {
        if ((Properties[I].queueFlags & RequiredFlags) == RequiredFlags)
        {
            Family = I;
            return true;
        }
    }
    return false;
}

uint32_t PhysicalDevice::FindMemoryType(uint32_t TypeFilter,
                                        VkMemoryPropertyFlags Properties) const
{
    VkPhysicalDeviceMemoryProperties MemProperties;
    vkGetPhysicalDeviceMemoryProperties(Handle, &MemProperties);

    for (uint32_t I = 0; I < MemProperties.memoryTypeCount; I++)
    {
        if ((TypeFilter & (1 << I)) &&
            (MemProperties.memoryTypes[I].propertyFlags & Properties) == Properties)
        {
            return I;
        }
    }

    RTRT_FATAL("No memory type matches filter 0x%x with properties 0x%x", TypeFilter,
               Properties);
}

bool PhysicalDevice::SupportsExtensions(
    VkPhysicalDevice Candidate, const std::vector<const char*>& RequiredExtensions)
{
    uint32_t ExtCount;
    RTRT_CHECK_VK(
        vkEnumerateDeviceExtensionProperties(Candidate, nullptr, &ExtCount, nullptr));
    std::vector<VkExtensionProperties> ExtensionProperties(ExtCount);
    RTRT_CHECK_VK(vkEnumerateDeviceExtensionProperties(Candidate, nullptr, &ExtCount,
                                                       ExtensionProperties.data()));

    printf("Required Extension Properties for GPU:\n");
    size_t Count = 0;
    for (const auto& Required : RequiredExtensions)
    {
        for (const auto& Properties : ExtensionProperties)
        {
            if (strcmp(Required, Properties.extensionName) == 0)
            {
                printf("%s matches the Required Device Extensions list!\n",
                       Properties.extensionName);
                ++Count;
                break;
            }
        }
    }

    return Count == RequiredExtensions.size();
}

}  // namespace rtrt
