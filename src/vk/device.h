#ifndef RTRT_SRC_VK_DEVICE_H_
#define RTRT_SRC_VK_DEVICE_H_

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "vk/context.h"
#include "vk/physical_device.h"
#include "vk_check.h"

namespace rtrt
{

// Names Object in validation messages and debuggers.
template <typename Handle>
void SetDebugName(VkDevice LogicalDevice, Handle Object, VkObjectType Type,
                  const char* Name)
{
    VkDebugUtilsObjectNameInfoEXT Info{
        VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    Info.objectType = Type;
    Info.objectHandle = reinterpret_cast<uint64_t>(Object);
    Info.pObjectName = Name;
    RTRT_CHECK_VK(vkSetDebugUtilsObjectNameEXT(LogicalDevice, &Info));
}

// The logical device with the ray tracing features on, its one queue, and the
// extension functions loaded for it.
class Device
{
public:
    Device() = default;
    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Picks the GPU, creates the device and its queue and loads the extension
    // functions. False when no GPU supports ray tracing.
    bool Init(const Context& Instance);

    // Index of a memory type allowed by TypeFilter that has all of Properties.
    uint32_t FindMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags Properties) const
    {
        return Gpu.FindMemoryType(TypeFilter, Properties);
    }

    VkDevice GetHandle() const
    {
        return Handle;
    }

    VkPhysicalDevice GetPhysical() const
    {
        return Gpu.GetHandle();
    }

    uint32_t GetQueueFamily() const
    {
        return QueueFamily;
    }

    VkQueue GetQueue() const
    {
        return Queue;
    }

private:
    void CreateLogicalDevice();
    void CheckPresentSupport(VkSurfaceKHR Surface) const;

    std::vector<const char*> ReqDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        // Required by the ray tracing pipeline
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};

    PhysicalDevice Gpu;
    uint32_t QueueFamily = VK_QUEUE_FAMILY_IGNORED;
    VkDevice Handle = VK_NULL_HANDLE;
    VkQueue Queue = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_DEVICE_H_
