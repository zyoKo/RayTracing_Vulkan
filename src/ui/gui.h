#ifndef RTRT_SRC_UI_GUI_H_
#define RTRT_SRC_UI_GUI_H_

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <future>
#include <string>

#include <imgui.h>
#include <vulkan/vulkan_core.h>

#include "core/window.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "scene/camera_controller.h"
#include "ui/editor_log.h"
#include "ui/theme.h"
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
    float GpuMs[kFrameCount] = {};
    int Next = 0;
    int Count = 0;

    void Push(float Frame, float Cpu, float Draw, float GpuWait, float Gpu)
    {
        FrameMs[Next] = Frame;
        CpuMs[Next] = Cpu;
        DrawMs[Next] = Draw;
        GpuWaitMs[Next] = GpuWait;
        GpuMs[Next] = Gpu;
        Next = (Next + 1) % kFrameCount;
        Count = std::min(Count + 1, kFrameCount);
    }

    float Latest(const float* Values) const
    {
        return Values[(Next + kFrameCount - 1) % kFrameCount];
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

// Names of the kView* modes in shared_structs.h, in order.
inline constexpr const char* kViewModeNames[] = {"Final", "Albedo", "Normal", "Depth",
                                                 "Sample count"};
inline constexpr int kViewModeCount = 5;

// What the outliner has selected and the inspector shows.
enum class Selection
{
    kNone,
    kCamera,
    kModel,
    kMaterial,
};

// The editor: ImGui context and backends, a dockspace with the viewport, outliner,
// inspector, render settings, stats and console panels, a menu bar and a status bar.
class Gui
{
public:
    Gui() = default;
    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;

    // Creates the ImGui context and its GLFW and Vulkan backends, drawing in Pass. Call
    // after the window's input handlers are set, so ImGui passes input on to them.
    void Init(const Window& Target, const Context& Instance, const Device& Gpu,
              VkRenderPass Pass, uint32_t ImageCount, Renderer& VK);

    // Waits for the GPU, then frees the backends and the context.
    void Shutdown();

    // Starts a UI frame and draws the editor.
    void Draw(Renderer& VK, Camera& View, CameraController& Controller);

    // Adds one frame's timings, in milliseconds, to the performance history.
    void RecordFrameTimes(float FrameMs, float CpuMs, float DrawMs, float GpuWaitMs,
                          float GpuMs)
    {
        Stats.Push(FrameMs, CpuMs, DrawMs, GpuWaitMs, GpuMs);
    }

    // True while ImGui is taking keyboard text, such as a Ctrl+clicked slider.
    static bool WantsTextInput();

    // True while a left-button drag that started on the viewport is under way.
    bool GetCameraOwnsMouse() const
    {
        return ViewportDragging;
    }

    // True while camera keys should move the camera: the viewport is hovered or focused.
    bool GetCameraOwnsKeys() const
    {
        return ViewportActive && !WantsTextInput();
    }

    // Set by File > Exit.
    bool GetExitRequested() const
    {
        return ExitRequested;
    }

private:
    void Log(LogLevel Level, const char* Format, ...);

    // Keeps panel visibility and the selection in imgui.ini.
    void RegisterSettingsHandler();
    void ReadSetting(const char* Line);
    void WriteSettings(ImGuiTextBuffer* Out) const;

    void HandleShortcuts(Renderer& VK);
    void RequestScreenshot(Renderer& VK);
    void TrackRendererEvents(const Renderer& VK);

    void DrawMenuBar(Renderer& VK, Camera& View);
    void DrawStatusBar(const Renderer& VK);
    void DrawDockSpace();
    void BuildDefaultLayout(ImGuiID DockId);
    void DrawControlsPopup();

    void SyncViewportTexture(const Renderer& VK);
    void DrawViewport(Renderer& VK, CameraController& Controller);
    void DrawViewportOverlay(Renderer& VK, const ImVec2& Origin, const ImVec2& Size);
    void DrawFullWindowViewport(Renderer& VK);

    void DrawOutliner(Renderer& VK);
    void DrawInspector(Renderer& VK, Camera& View, CameraController& Controller);
    void DrawCameraInspector(Camera& View, CameraController& Controller);
    void DrawModelInspector(Renderer& VK);
    void DrawMaterialInspector(Renderer& VK);

    void DrawRenderSettings(Renderer& VK);
    // Each returns true when a change makes the accumulated history wrong.
    bool DrawIntegratorSettings(Renderer& VK);
    bool DrawAccumulationSettings(Renderer& VK);
    void DrawDenoiserSettings(Renderer& VK);
    void DrawDisplaySettings(Renderer& VK);
    void DrawShaderSettings(Renderer& VK);

    void DrawStats(const Renderer& VK);
    void DrawConsole();

    VkDevice DeviceHandle = VK_NULL_HANDLE;
    std::string GpuName;
    EditorFonts Fonts;
    EditorLog Messages;
    double StartTime = 0.0;

    VkDescriptorSet ViewportTexture = VK_NULL_HANDLE;
    uint32_t ViewportVersion = 0;
    bool ViewportDragging = false;
    bool ViewportActive = false;

    bool ShowUi = true;
    bool ShowOutliner = true;
    bool ShowInspector = true;
    bool ShowRenderSettings = true;
    bool ShowStats = true;
    bool ShowConsole = true;
    bool ResetLayout = false;
    bool OpenControls = false;
    bool ExitRequested = false;

    Selection Selected = Selection::kCamera;
    uint32_t SelectedMesh = 0;
    uint32_t SelectedMaterial = 0;
    ImGuiTextFilter MaterialFilter;
    ImGuiTextFilter ConsoleFilter;
    size_t ConsoleSeen = 0;

    uint32_t ScreenshotsSeen = 0;
    bool LastUseSlang = true;
    bool LastUseRaytracer = true;
    VkExtent2D LoggedExtent{0, 0};
    double ExtentChangedAt = 0.0;

    // GLSL was picked before its shaders were built
    bool WantGlsl = false;
    std::future<bool> GlslBuild;
    bool GlslBuildFailed = false;
    FrameStats Stats;
};

}  // namespace rtrt

#endif  // RTRT_SRC_UI_GUI_H_
