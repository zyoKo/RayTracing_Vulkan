#ifndef RTRT_SRC_RENDERER_GPU_TIMER_H_
#define RTRT_SRC_RENDERER_GPU_TIMER_H_

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "vk/device.h"

namespace rtrt
{

// One timed GPU section of a frame.
struct GpuScope
{
    const char* Name = "";
    float Ms = 0.0f;
};

// Times named sections of the frame's command buffer with timestamp queries. Results
// are read one frame late, after the frame fence, so reading never stalls.
class GpuTimer
{
public:
    GpuTimer() = default;
    ~GpuTimer();
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;

    void Create(const Device& Gpu, uint32_t MaxScopes);
    void Destroy();

    // Reads the last recorded frame's timings. Call once its fence has signalled.
    void Resolve();

    // Resets the queries; record outside a render pass, before the first Begin.
    void BeginFrame(VkCommandBuffer Cmd);
    // Name must outlive the timer, such as a string literal.
    void Begin(VkCommandBuffer Cmd, const char* Name);
    void End(VkCommandBuffer Cmd);

    // Sections of the last resolved frame, in recording order.
    const std::vector<GpuScope>& GetScopes() const
    {
        return Scopes;
    }

    // From the first Begin to the last End of the last resolved frame.
    float GetFrameMs() const
    {
        return FrameMs;
    }

private:
    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkQueryPool Pool = VK_NULL_HANDLE;
    uint32_t MaxScopes = 0;
    // Nanoseconds per timestamp tick
    float Period = 1.0f;
    std::vector<const char*> Recorded;
    bool Pending = false;
    std::vector<GpuScope> Scopes;
    float FrameMs = 0.0f;
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_GPU_TIMER_H_
