#ifndef RTRT_SRC_UI_GUI_H_
#define RTRT_SRC_UI_GUI_H_

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <future>

#include <vulkan/vulkan_core.h>

#include "core/window.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "scene/camera_controller.h"
#include "vk/context.h"
#include "vk/device.h"

namespace rtrt
{

// The last kFrameCount frame timings, in milliseconds, kept as a ring buffer.
struct FrameStats
{
    static constexpr int kFrameCount = 240;
    float FrameMs[kFrameCount] = {};
    float CpuMs[kFrameCount] = {};
    float DrawMs[kFrameCount] = {};
    float GpuWaitMs[kFrameCount] = {};
    int Next = 0;
    int Count = 0;

    void Push(float Frame, float Cpu, float Draw, float GpuWait)
    {
        FrameMs[Next] = Frame;
        CpuMs[Next] = Cpu;
        DrawMs[Next] = Draw;
        GpuWaitMs[Next] = GpuWait;
        Next = (Next + 1) % kFrameCount;
        Count = std::min(Count + 1, kFrameCount);
    }

    struct Summary
    {
        float Avg = 0.0f;
        float Lo = 0.0f;
        float Hi = 0.0f;
    };

    Summary Summarize(const float* Values) const
    {
        Summary Result;
        if (Count == 0)
        {
            return Result;
        }
        Result.Lo = FLT_MAX;
        for (int I = 0; I < Count; I++)
        {
            Result.Avg += Values[I];
            Result.Lo = std::min(Result.Lo, Values[I]);
            Result.Hi = std::max(Result.Hi, Values[I]);
        }
        Result.Avg /= Count;
        return Result;
    }
};

// The ImGui context and backends, and the settings and performance windows.
class Gui
{
public:
    Gui() = default;
    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;

    // Creates the ImGui context and its GLFW and Vulkan backends, drawing in Pass. Call
    // after the window's input handlers are set, so ImGui passes input on to them.
    void Init(const Window& Target, const Context& Instance, const Device& Gpu,
              VkRenderPass Pass, uint32_t ImageCount);

    // Waits for the GPU, then frees the backends and the context.
    void Shutdown();

    // Starts a UI frame and draws the windows that are switched on.
    void Draw(Renderer& VK, Camera& View, CameraController& Controller);

    // Adds one frame's timings, in milliseconds, to the performance history.
    void RecordFrameTimes(float FrameMs, float CpuMs, float DrawMs, float GpuWaitMs)
    {
        Stats.Push(FrameMs, CpuMs, DrawMs, GpuWaitMs);
    }

    void ToggleSettings()
    {
        ShowGui = !ShowGui;
    }

    // True while ImGui is taking keyboard text, such as a Ctrl+clicked slider.
    static bool WantsTextInput();
    // True while the mouse is over an ImGui window.
    static bool WantsMouse();

private:
    void DrawSettingsWindow(Renderer& VK, Camera& View, CameraController& Controller);
    void DrawPerfWindow(const Renderer& VK);

    // Each returns true when a change makes the accumulated history wrong.
    bool DrawModeSettings(Renderer& VK);
    bool DrawPathTracerSettings(Renderer& VK);
    bool DrawHistorySettings(Renderer& VK);

    void DrawDenoiserSettings(Renderer& VK);
    void DrawDisplaySettings(Renderer& VK);
    void DrawCameraSettings(Camera& View, CameraController& Controller);
    void DrawShaderSettings(Renderer& VK);

    // Registers the viewport image with ImGui again after the renderer rebuilds it.
    void SyncViewportTexture(const Renderer& VK);
    void DrawViewportBackground(Renderer& VK);

    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkDescriptorSet ViewportTexture = VK_NULL_HANDLE;
    uint32_t ViewportVersion = 0;
    // GLSL was picked before its shaders were built
    bool WantGlsl = false;
    std::future<bool> GlslBuild;
    bool GlslBuildFailed = false;
    bool ShowGui = true;
    bool ShowPerf = false;
    FrameStats Stats;
};

}  // namespace rtrt

#endif  // RTRT_SRC_UI_GUI_H_
