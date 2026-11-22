#include "renderer/gpu_timer.h"

#include <cstdint>
#include <vector>

#include "vk_check.h"

namespace rtrt
{

GpuTimer::~GpuTimer()
{
    Destroy();
}

void GpuTimer::Create(const Device& Gpu, uint32_t ScopeCount)
{
    DeviceHandle = Gpu.GetHandle();
    MaxScopes = ScopeCount;

    VkPhysicalDeviceProperties Properties{};
    vkGetPhysicalDeviceProperties(Gpu.GetPhysical(), &Properties);
    Period = Properties.limits.timestampPeriod;

    VkQueryPoolCreateInfo Info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    Info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    Info.queryCount = MaxScopes * 2;
    RTRT_CHECK_VK(vkCreateQueryPool(DeviceHandle, &Info, nullptr, &Pool));
    Recorded.reserve(MaxScopes);
}

void GpuTimer::Destroy()
{
    if (Pool != VK_NULL_HANDLE)
    {
        vkDestroyQueryPool(DeviceHandle, Pool, nullptr);
        Pool = VK_NULL_HANDLE;
    }
    Recorded.clear();
    Pending = false;
}

void GpuTimer::Resolve()
{
    if (!Pending || Recorded.empty())
    {
        return;
    }
    Pending = false;

    const auto Count = static_cast<uint32_t>(Recorded.size()) * 2;
    std::vector<uint64_t> Ticks(Count);
    const VkResult Result =
        vkGetQueryPoolResults(DeviceHandle, Pool, 0, Count, Count * sizeof(uint64_t),
                              Ticks.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    if (Result == VK_NOT_READY)
    {
        return;
    }
    RTRT_CHECK_VK(Result);

    const float MsPerTick = Period * 1e-6f;
    Scopes.resize(Recorded.size());
    for (size_t I = 0; I < Recorded.size(); I++)
    {
        Scopes[I].Name = Recorded[I];
        Scopes[I].Ms = static_cast<float>(Ticks[2 * I + 1] - Ticks[2 * I]) * MsPerTick;
    }
    FrameMs = static_cast<float>(Ticks[Count - 1] - Ticks[0]) * MsPerTick;
}

void GpuTimer::BeginFrame(VkCommandBuffer Cmd)
{
    vkCmdResetQueryPool(Cmd, Pool, 0, MaxScopes * 2);
    Recorded.clear();
    Pending = true;
}

void GpuTimer::Begin(VkCommandBuffer Cmd, const char* Name)
{
    if (Recorded.size() >= MaxScopes)
    {
        return;
    }
    vkCmdWriteTimestamp(Cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, Pool,
                        static_cast<uint32_t>(Recorded.size()) * 2);
    Recorded.push_back(Name);
}

void GpuTimer::End(VkCommandBuffer Cmd)
{
    if (Recorded.empty())
    {
        return;
    }
    vkCmdWriteTimestamp(Cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Pool,
                        static_cast<uint32_t>(Recorded.size()) * 2 - 1);
}

}  // namespace rtrt
