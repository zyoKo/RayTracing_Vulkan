#ifndef RTRT_SRC_RT_SHADER_BINDING_TABLE_H_
#define RTRT_SRC_RT_SHADER_BINDING_TABLE_H_

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "vk/buffer.h"
#include "vk/command_pool.h"
#include "vk/device.h"

namespace rtrt
{

// The shader binding table of a ray tracing pipeline whose groups are one raygen, then
// the miss groups, then the hit groups.
class ShaderBindingTable
{
public:
    // Reads the group handles from Pipeline and uploads them, aligned for this GPU.
    void Create(const Device& Gpu, const CommandPool& Pool, VkPipeline Pipeline,
                uint32_t MissCount, uint32_t HitCount);
    void Destroy();

    // Records a trace of Width x Height rays through this table.
    void CmdTraceRays(VkCommandBuffer Cmd, uint32_t Width, uint32_t Height) const;

private:
    // Fills the stride and size of each region from the handle size and alignments.
    void ComputeRegions(uint32_t MissCount, uint32_t HitCount);
    // Lays the handles out at the region offsets, ready to upload.
    std::vector<uint8_t> LayOutHandles(const std::vector<uint8_t>& Handles,
                                       uint32_t MissCount, uint32_t HitCount) const;

    uint32_t HandleSize = 0;
    uint32_t HandleAlignment = 0;
    uint32_t BaseAlignment = 0;
    Buffer Table;
    VkStridedDeviceAddressRegionKHR RgenRegion{};
    VkStridedDeviceAddressRegionKHR MissRegion{};
    VkStridedDeviceAddressRegionKHR HitRegion{};
    VkStridedDeviceAddressRegionKHR CallRegion{};
};

}  // namespace rtrt

#endif  // RTRT_SRC_RT_SHADER_BINDING_TABLE_H_
