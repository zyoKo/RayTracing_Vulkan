#ifndef RTRT_SRC_CORE_APPLICATION_H_
#define RTRT_SRC_CORE_APPLICATION_H_

#include <algorithm>
#include <cfloat>

#include "core/window.h"
#include "renderer.h"
#include "scene/camera.h"
#include "scene/camera_controller.h"
#include "vk/context.h"
#include "vk/device.h"
#include "vk/swapchain.h"

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

// Owns the window, the Vulkan objects, the camera and the renderer, runs the frame
// loop, and draws the ImGui windows on top of the renderer.
class Application
{
public:
    Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Reads the command line, opens the window and sets up Vulkan and the renderer.
    // False on a bad argument, or when GLFW, Vulkan or a ray tracing GPU is missing.
    bool Init(int Argc, char** Argv);

    // Runs the frame loop until the window closes, then frees the Vulkan objects.
    // Returns the process exit code.
    int Run();

    // Moves the camera for the keys and mouse button held down since the last frame.
    void UpdateCamera();

    // Draws the settings and performance windows that are switched on.
    void DrawGui(Renderer& VK);

    // Adds one frame's timings, in milliseconds, to the performance history.
    void RecordFrameTimes(float FrameMs, float CpuMs, float DrawMs, float GpuWaitMs);

private:
    void OnKey(int Key, int Action);
    void OnMouseButton(double X, double Y);
    void OnCursorPos(double X, double Y);

    void DrawSettingsWindow(Renderer& VK);
    void DrawPerfWindow();

    // Each returns true when a change makes the accumulated history wrong.
    bool DrawModeSettings(Renderer& VK);
    bool DrawPathTracerSettings(Renderer& VK);
    bool DrawHistorySettings(Renderer& VK);

    void DrawDenoiserSettings(Renderer& VK);
    void DrawDisplaySettings(Renderer& VK);
    void DrawCameraSettings();
    void DrawShaderSettings(Renderer& VK);

    Window MainWindow;
    Context GpuContext;
    Device GpuDevice;
    Swapchain MainSwapchain;
    bool DoApiDump = false;
    // -slang: start with the Slang shaders
    bool UseSlang = false;

    bool ShowGui = true;
    bool ShowPerf = false;
    Camera MainCamera;
    CameraController MainController;
    FrameStats Stats;
    Renderer MainRenderer;
};

}  // namespace rtrt

#endif  // RTRT_SRC_CORE_APPLICATION_H_
