#include "rt/shader_binding_table.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "vk_check.h"

namespace rtrt
{

namespace
{

template <class Integral>
constexpr Integral AlignUp(Integral X, size_t A) noexcept
{
    return static_cast<Integral>((X + (static_cast<Integral>(A) - 1)) &
                                 ~static_cast<Integral>(A - 1));
}

void PrintSbtRegion(const char* Name, const VkStridedDeviceAddressRegionKHR& Region)
{
    printf("    %s %2llu:%2llu\n", Name, static_cast<unsigned long long>(Region.stride),
           static_cast<unsigned long long>(Region.size));
}

}  // namespace

void ShaderBindingTable::Create(const Device& Gpu, const CommandPool& Pool,
                                VkPipeline Pipeline, uint32_t MissCount,
                                uint32_t HitCount)
{
    VkPhysicalDeviceProperties2 Prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR RtProps{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    Prop2.pNext = &RtProps;
    vkGetPhysicalDeviceProperties2(Gpu.GetPhysical(), &Prop2);
    HandleSize = RtProps.shaderGroupHandleSize;
    HandleAlignment = RtProps.shaderGroupHandleAlignment;
    BaseAlignment = RtProps.shaderGroupBaseAlignment;

    ComputeRegions(MissCount, HitCount);

    const uint32_t HandleCount = 1 + MissCount + HitCount;
    const uint32_t DataSize = HandleCount * HandleSize;
    std::vector<uint8_t> Handles(DataSize);
    RTRT_CHECK_VK(vkGetRayTracingShaderGroupHandlesKHR(
        Gpu.GetHandle(), Pipeline, 0, HandleCount, DataSize, Handles.data()));

    Table.CreateWithData(Gpu, Pool, LayOutHandles(Handles, MissCount, HitCount),
                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                             VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR);

    const VkDeviceAddress SbtAddress = Table.GetAddress();
    RgenRegion.deviceAddress = SbtAddress;
    MissRegion.deviceAddress = SbtAddress + RgenRegion.size;
    HitRegion.deviceAddress = SbtAddress + RgenRegion.size + MissRegion.size;
}

void ShaderBindingTable::Destroy()
{
    Table.Destroy();
}

void ShaderBindingTable::CmdTraceRays(VkCommandBuffer Cmd, uint32_t Width,
                                      uint32_t Height) const
{
    vkCmdTraceRaysKHR(Cmd, &RgenRegion, &MissRegion, &HitRegion, &CallRegion, Width,
                      Height, 1);
}

void ShaderBindingTable::ComputeRegions(uint32_t MissCount, uint32_t HitCount)
{
    const uint32_t HandleCount = 1 + MissCount + HitCount;

    // Each handle is aligned to HandleAlignment and each region starts at a multiple of
    // BaseAlignment.
    const uint32_t HandleSizeAligned = AlignUp(HandleSize, HandleAlignment);

    RgenRegion.stride = AlignUp(HandleSizeAligned, BaseAlignment);
    // The raygen region's size must equal its stride
    RgenRegion.size = RgenRegion.stride;

    MissRegion.stride = HandleSizeAligned;
    MissRegion.size = AlignUp(MissCount * HandleSizeAligned, BaseAlignment);

    HitRegion.stride = HandleSizeAligned;
    HitRegion.size = AlignUp(HitCount * HandleSizeAligned, BaseAlignment);

    printf("Shader binding table:\n");
    printf("  alignments:\n");
    printf("    handleAlignment: %d\n", HandleAlignment);
    printf("    baseAlignment:   %d\n", BaseAlignment);
    printf("  counts:\n");
    printf("    miss:   %d\n", MissCount);
    printf("    hit:    %d\n", HitCount);
    printf("    handle: %d = 1+missCount+hitCount\n", HandleCount);
    printf("  regions stride:size:\n");
    PrintSbtRegion("rgen", RgenRegion);
    PrintSbtRegion("miss", MissRegion);
    PrintSbtRegion("hit ", HitRegion);
    PrintSbtRegion("call", CallRegion);
}

std::vector<uint8_t> ShaderBindingTable::LayOutHandles(
    const std::vector<uint8_t>& Handles, uint32_t MissCount, uint32_t HitCount) const
{
    const VkDeviceSize SbtSize =
        RgenRegion.size + MissRegion.size + HitRegion.size + CallRegion.size;
    std::vector<uint8_t> Data(static_cast<size_t>(SbtSize));
    auto HandleAt = [&](uint32_t I) { return Handles.data() + I * HandleSize; };

    uint32_t HandleIdx = 0;
    memcpy(Data.data(), HandleAt(HandleIdx++), HandleSize);

    VkDeviceSize Offset = RgenRegion.size;
    for (uint32_t C = 0; C < MissCount; C++)
    {
        memcpy(Data.data() + Offset, HandleAt(HandleIdx++), HandleSize);
        Offset += MissRegion.stride;
    }

    Offset = RgenRegion.size + MissRegion.size;
    for (uint32_t C = 0; C < HitCount; C++)
    {
        memcpy(Data.data() + Offset, HandleAt(HandleIdx++), HandleSize);
        Offset += HitRegion.stride;
    }
    return Data;
}

}  // namespace rtrt
