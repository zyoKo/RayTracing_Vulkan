#include "ui/gui.h"

#include <algorithm>
#include <cstdint>
#include <thread>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#include <imgui.h>

#include "vk/command_pool.h"
#include "vk_check.h"

namespace rtrt
{

namespace
{

constexpr float kSettingsWidth = 340.0f;
constexpr float kSettingsMinWidth = 260.0f;

void StatRow(const FrameStats& Stats, const char* Label, const float* Values)
{
    const FrameStats::Summary Range = Stats.Summarize(Values);
    ImGui::TextUnformatted(Label);
    ImGui::NextColumn();
    ImGui::Text(
        "%.2f",
        Values[(Stats.Next + FrameStats::kFrameCount - 1) % FrameStats::kFrameCount]);
    ImGui::NextColumn();
    ImGui::Text("%.2f", Range.Avg);
    ImGui::NextColumn();
    ImGui::Text("%.2f", Range.Lo);
    ImGui::NextColumn();
    ImGui::Text("%.2f", Range.Hi);
    ImGui::NextColumn();
}

}  // namespace

void Gui::Init(const Window& Target, const Context& Instance, const Device& Gpu,
               VkRenderPass Pass, uint32_t ImageCount)
{
    DeviceHandle = Gpu.GetHandle();

    ImGui::CreateContext();
    ImGuiIO& Io = ImGui::GetIO();
    Io.LogFilename = nullptr;
    Io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    ImguiDescPool.Create(
        Gpu.GetHandle(),
        {{VK_DESCRIPTOR_TYPE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}},
        2);

    // Setup Platform/Renderer back ends
    ImGui_ImplVulkan_InitInfo InitInfo = {};
    InitInfo.Instance = Instance.GetInstance();
    InitInfo.PhysicalDevice = Gpu.GetPhysical();
    InitInfo.Device = Gpu.GetHandle();
    InitInfo.QueueFamily = Gpu.GetQueueFamily();
    InitInfo.Queue = Gpu.GetQueue();
    InitInfo.PipelineCache = VK_NULL_HANDLE;
    InitInfo.DescriptorPool = ImguiDescPool.GetHandle();
    InitInfo.Subpass = 0;
    InitInfo.MinImageCount = 2;
    InitInfo.ImageCount = ImageCount;
    InitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    InitInfo.CheckVkResultFn = nullptr;
    InitInfo.Allocator = nullptr;

    ImGui_ImplVulkan_Init(&InitInfo, Pass);

    // The font atlas is uploaded once, through a pool that only lives for the upload
    CommandPool UploadPool;
    UploadPool.Create(Gpu);
    VkCommandBuffer Cmdbuf = UploadPool.BeginSingleUse();
    ImGui_ImplVulkan_CreateFontsTexture(Cmdbuf);
    UploadPool.EndSingleUse(Cmdbuf);
    ImGui_ImplVulkan_DestroyFontUploadObjects();  // Staging buffer is no longer needed

    ImGui_ImplGlfw_InitForVulkan(Target.GetHandle(), true);
}

void Gui::Shutdown()
{
    RTRT_CHECK_VK(vkDeviceWaitIdle(DeviceHandle));
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    ImguiDescPool.Destroy();
}

bool Gui::WantsTextInput()
{
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput;
}

bool Gui::WantsMouse()
{
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
}

void Gui::Draw(Renderer& VK, Camera& View, CameraController& Controller)
{
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ShowGui)
    {
        DrawSettingsWindow(VK, View, Controller);
    }
    if (ShowPerf)
    {
        DrawPerfWindow();
    }
}

void Gui::DrawPerfWindow()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 330), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Performance", &ShowPerf))
    {
        ImGui::End();
        return;
    }

    const FrameStats::Summary Frame = Stats.Summarize(Stats.FrameMs);
    const float AvgFrame = Frame.Avg;
    ImGui::Text("FPS: %.1f   Frame latency: %.2f ms",
                AvgFrame > 0.0f ? 1000.0f / AvgFrame : 0.0f, AvgFrame);

    char Overlay[32];
    snprintf(Overlay, sizeof(Overlay), "%.2f ms", AvgFrame);
    ImGui::PlotLines("##frame", Stats.FrameMs, FrameStats::kFrameCount, Stats.Next,
                     Overlay, 0.0f, std::max(Frame.Hi * 1.2f, 1.0f),
                     ImVec2(ImGui::GetContentRegionAvail().x, 70));

    ImGui::Columns(5, "timings");
    ImGui::SetColumnWidth(0, 110);
    for (const char* Header : {"ms", "Now", "Avg", "Min", "Max"})
    {
        ImGui::TextUnformatted(Header);
        ImGui::NextColumn();
    }
    ImGui::Separator();
    StatRow(Stats, "Frame", Stats.FrameMs);
    StatRow(Stats, "CPU thread", Stats.CpuMs);
    StatRow(Stats, "Draw thread", Stats.DrawMs);
    StatRow(Stats, "GPU wait", Stats.GpuWaitMs);
    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::TextWrapped(
        "CPU thread: input, camera and UI. Draw thread: acquire, record, "
        "submit and present. Both run on the main thread; GPU wait is "
        "time blocked on the previous frame's fence.");
    ImGui::Text("Hardware threads: %u", std::thread::hardware_concurrency());
    ImGui::Text("Averages over the last %d frames", Stats.Count);

    ImGui::End();
}

void Gui::DrawSettingsWindow(Renderer& VK, Camera& View, CameraController& Controller)
{
    // Pinned to the right edge at full height; only the width can change.
    const ImVec2 Display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(Display.x, 0.0f), ImGuiCond_Always,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(kSettingsWidth, Display.y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(kSettingsMinWidth, Display.y),
        ImVec2(std::max(Display.x * 0.5f, kSettingsMinWidth), Display.y));
    if (!ImGui::Begin("Settings (F1 to hide)", &ShowGui,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }
    ImGui::PushItemWidth(150);

    const ImGuiIO& Io = ImGui::GetIO();
    ImGui::Text("%.1f FPS (%.2f ms)", Io.Framerate, 1000.0f / Io.Framerate);
    ImGui::Text("Frames since reset: %d", VK.GetFrameCount());

    // Anything that changes what a path returns makes the existing history wrong.
    bool ResetHistory = DrawModeSettings(VK);
    ResetHistory |= DrawPathTracerSettings(VK);
    ResetHistory |= DrawHistorySettings(VK);
    DrawDenoiserSettings(VK);
    DrawDisplaySettings(VK);
    DrawCameraSettings(View, Controller);
    DrawShaderSettings(VK);

    ImGui::Separator();
    ImGui::Checkbox("Show performance window", &ShowPerf);
    if (ImGui::Button("Restore default settings"))
    {
        VK.SetDefaultSettings();
    }

    if (ResetHistory)
    {
        VK.ResetRtAccumulation();
    }

    ImGui::PopItemWidth();
    ImGui::End();
}

bool Gui::DrawModeSettings(Renderer& VK)
{
    RenderSettings& Settings = VK.GetSettings();
    bool ResetHistory = false;

    if (ImGui::RadioButton("Ray tracer", Settings.UseRaytracer))
    {
        ResetHistory |= !Settings.UseRaytracer;
        Settings.UseRaytracer = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rasterizer", !Settings.UseRaytracer))
    {
        Settings.UseRaytracer = false;
    }

    const char* ViewNames[] = {"Final", "Albedo", "Normal", "Depth", "Sample count"};
    ImGui::Combo("View", &Settings.ViewMode, ViewNames, IM_ARRAYSIZE(ViewNames));
    return ResetHistory;
}

bool Gui::DrawPathTracerSettings(Renderer& VK)
{
    if (!ImGui::CollapsingHeader("Path tracer", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return false;
    }

    PushConstantRay& Ray = VK.GetRayConstants();
    bool ResetHistory = ImGui::Checkbox("Specular", &Ray.FullBrdf);
    ResetHistory |= ImGui::SliderFloat("Survival chance", &Ray.Rr, 0.1f, 0.95f, "%.2f");
    ResetHistory |= ImGui::SliderInt("Max path length", &Ray.Depth, 1, 32);
    ResetHistory |=
        ImGui::SliderFloat("Emission scale", &Ray.EmissionScale, 0.0f, 5.0f, "%.2f");
    return ResetHistory;
}

bool Gui::DrawHistorySettings(Renderer& VK)
{
    if (!ImGui::CollapsingHeader("Temporal history", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return false;
    }

    PushConstantRay& Ray = VK.GetRayConstants();
    ImGui::Checkbox("Use history", &Ray.UseHistory);
    ImGui::SliderFloat("Normal threshold", &Ray.NormalThreshold, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Depth threshold", &Ray.DepthThreshold, 0.001f, 0.2f, "%.3f");
    ImGui::SliderFloat("Max samples (0 = off)", &Ray.MaxSamples, 0.0f, 4096.0f, "%.0f",
                       ImGuiSliderFlags_Logarithmic);
    return ImGui::Button("Reset history");
}

void Gui::DrawDenoiserSettings(Renderer& VK)
{
    if (!ImGui::CollapsingHeader("Denoiser", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    RenderSettings& Settings = VK.GetSettings();
    PushConstantDenoise& Denoise = VK.GetDenoiseConstants();
    ImGui::Checkbox("Enabled", &Settings.DenoiseEnabled);
    ImGui::SliderInt("Iterations", &Settings.NumAtrousIterations, 0, 8);
    ImGui::SliderFloat("Normal factor", &Denoise.NormFactor, 0.0f, 0.1f, "%.4f",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Depth factor", &Denoise.DepthFactor, 0.0f, 0.1f, "%.4f",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Albedo factor", &Denoise.AlbedoFactor, 0.0f, 1.0f, "%.3f",
                       ImGuiSliderFlags_Logarithmic);
}

void Gui::DrawDisplaySettings(Renderer& VK)
{
    if (!ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    RenderSettings& Settings = VK.GetSettings();
    ImGui::SliderFloat("Exposure", &Settings.Exposure, 0.1f, 4.0f, "%.2f");
    ImGui::SliderFloat("Gamma", &Settings.Gamma, 1.0f, 3.0f, "%.2f");
}

void Gui::DrawCameraSettings(Camera& View, CameraController& Controller)
{
    if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    const glm::vec3& Eye = View.GetEye();
    ImGui::Text("Position: %.2f, %.2f, %.2f", Eye.x, Eye.y, Eye.z);
    float MoveSpeed = Controller.GetMoveSpeed();
    if (ImGui::SliderFloat("Move speed", &MoveSpeed, 0.1f, 5.0f, "%.2f"))
    {
        Controller.SetMoveSpeed(MoveSpeed);
    }
    if (ImGui::Button("Reset camera"))
    {
        View.Reset();
    }
}

void Gui::DrawShaderSettings(Renderer& VK)
{
    if (!ImGui::CollapsingHeader("Shaders", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    if (ImGui::RadioButton("GLSL", !VK.GetUseSlang()))
    {
        VK.SetShaderLanguage(false);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Slang", VK.GetUseSlang()))
    {
        VK.SetShaderLanguage(true);
    }
}

}  // namespace rtrt
