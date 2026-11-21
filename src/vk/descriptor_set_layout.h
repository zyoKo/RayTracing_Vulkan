#ifndef RTRT_SRC_VK_DESCRIPTOR_SET_LAYOUT_H_
#define RTRT_SRC_VK_DESCRIPTOR_SET_LAYOUT_H_

#include <vector>

#include <vulkan/vulkan_core.h>

namespace rtrt
{

// A descriptor set layout and the bindings it was made from. Frees itself when
// destroyed.
class DescriptorSetLayout
{
public:
    DescriptorSetLayout() = default;
    ~DescriptorSetLayout();
    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

    // Binding I of Bindings must use binding number I.
    void Create(VkDevice LogicalDevice,
                const std::vector<VkDescriptorSetLayoutBinding>& Bindings);

    // Safe to call on an empty layout.
    void Destroy();

    const VkDescriptorSetLayout& GetHandle() const
    {
        return Handle;
    }

    const std::vector<VkDescriptorSetLayoutBinding>& GetBindings() const
    {
        return Bindings;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkDescriptorSetLayout Handle = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayoutBinding> Bindings;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_DESCRIPTOR_SET_LAYOUT_H_
