#ifndef RTRT_SRC_VK_DESCRIPTOR_SET_H_
#define RTRT_SRC_VK_DESCRIPTOR_SET_H_

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "vk/descriptor_pool.h"
#include "vk/descriptor_set_layout.h"
#include "vk/image.h"

namespace rtrt
{

// One descriptor set. The pool it came from frees it.
class DescriptorSet
{
public:
    void Allocate(VkDevice LogicalDevice, const DescriptorPool& Pool,
                  const DescriptorSetLayout& Layout);

    // Each writes one binding; the binding's type must match the data.
    void Write(uint32_t Index, VkBuffer BufferHandle) const;
    void Write(uint32_t Index, const VkDescriptorImageInfo& ImageInfo) const;
    void Write(uint32_t Index, const std::vector<Image>& Textures) const;
    void Write(uint32_t Index, VkAccelerationStructureKHR Tlas) const;

    const VkDescriptorSet& GetHandle() const
    {
        return Handle;
    }

private:
    // A write of Index with everything but the data filled in.
    VkWriteDescriptorSet MakeWrite(uint32_t Index, uint32_t Count) const;

    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkDescriptorSet Handle = VK_NULL_HANDLE;
    std::vector<VkDescriptorType> BindingTypes;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_DESCRIPTOR_SET_H_
