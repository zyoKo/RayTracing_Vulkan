#include "app.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <glm/glm.hpp>
#include <imgui.h>

#include "camera.h"
#include "renderer.h"

namespace rtrt
{

namespace
{

constexpr int kWidth = 10 * 128;
constexpr int kHeight = 6 * 128;
constexpr char kProject[] = "rtrt";

void OnErrorCallback(int Error, const char* Description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", Error, Description);
}

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

bool ImGuiWantsTextInput()
{
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput;
}

bool ImGuiWantsMouse()
{
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
}

}  // namespace

void App::FramebufferSizeCallback(GLFWwindow* Window, int Width, int Height)
{
    App* Application = static_cast<App*>(glfwGetWindowUserPointer(Window));
    Application->FramebufferResized = true;
}

void App::MouseButtonCallback(GLFWwindow* Window, int Button, int Action, int Mods)
{
    App* Application = static_cast<App*>(glfwGetWindowUserPointer(Window));
    if (ImGuiWantsMouse())
    {
        return;
    }

    double X, Y;
    glfwGetCursorPos(Window, &X, &Y);
    Application->MainCamera.SetMousePosition(X, Y);
}

void App::CursorPosCallback(GLFWwindow* Window, double X, double Y)
{
    App* Application = static_cast<App*>(glfwGetWindowUserPointer(Window));
    if (ImGuiWantsMouse())
    {
        return;
    }

    if (Application->MainCamera.GetLeftButtonDown())
    {
        Application->MainCamera.MouseMove(X, Y);
    }
}

void App::KeyCallback(GLFWwindow* Window, int Key, int Scancode, int Action, int Mods)
{
    App* Application = static_cast<App*>(glfwGetWindowUserPointer(Window));
    const bool Pressed = Action != GLFW_RELEASE;

    // Keys typed into a settings field (Ctrl+click on a slider) belong to that field.
    if (ImGuiWantsTextInput())
    {
        return;
    }

    if (Pressed && Key == GLFW_KEY_ESCAPE)
    {
        glfwSetWindowShouldClose(Window, 1);
    }

    if (Action == GLFW_PRESS && Key == GLFW_KEY_F1)
    {
        Application->ShowGui = !Application->ShowGui;
    }
}

void App::DrawGui(Renderer& VK)
{
    if (ShowGui)
    {
        DrawSettingsWindow(VK);
    }
    if (ShowPerf)
    {
        DrawPerfWindow();
    }
}

void App::RecordFrameTimes(float FrameMs, float CpuMs, float DrawMs, float GpuWaitMs)
{
    Stats.Push(FrameMs, CpuMs, DrawMs, GpuWaitMs);
}

void App::DrawPerfWindow()
{
    ImGui::SetNextWindowPos(ImVec2(360, 10), ImGuiCond_FirstUseEver);
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

void App::DrawSettingsWindow(Renderer& VK)
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Settings (F1 to hide)", &ShowGui))
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
    DrawCameraSettings();
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

bool App::DrawModeSettings(Renderer& VK)
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

bool App::DrawPathTracerSettings(Renderer& VK)
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

bool App::DrawHistorySettings(Renderer& VK)
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

void App::DrawDenoiserSettings(Renderer& VK)
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

void App::DrawDisplaySettings(Renderer& VK)
{
    if (!ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    RenderSettings& Settings = VK.GetSettings();
    ImGui::SliderFloat("Exposure", &Settings.Exposure, 0.1f, 4.0f, "%.2f");
    ImGui::SliderFloat("Gamma", &Settings.Gamma, 1.0f, 3.0f, "%.2f");
}

void App::DrawCameraSettings()
{
    if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    const glm::vec3& Eye = MainCamera.GetEye();
    ImGui::Text("Position: %.2f, %.2f, %.2f", Eye.x, Eye.y, Eye.z);
    ImGui::SliderFloat("Move speed", &MoveSpeed, 0.1f, 5.0f, "%.2f");
    if (ImGui::Button("Reset camera"))
    {
        MainCamera.Reset();
    }
}

void App::DrawShaderSettings(Renderer& VK)
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

void App::UpdateCamera()
{
    float Now = glfwGetTime();
    float Dt = Now - LastTime;
    LastTime = Now;

    float Dist = MoveSpeed * Dt;

    MainCamera.SetLeftButtonDown(glfwGetMouseButton(Window, GLFW_MOUSE_BUTTON_LEFT) ==
                                 GLFW_PRESS);

    // Letters typed into a settings field (Ctrl+click on a slider) must not move the
    // camera.
    if (ImGuiWantsTextInput())
    {
        return;
    }

    const float Spin = glm::radians(MainCamera.GetSpin());
    const glm::vec3 Forward(sin(Spin), 0.0, -cos(Spin));
    const glm::vec3 Right(cos(Spin), 0.0, sin(Spin));

    if (glfwGetKey(Window, GLFW_KEY_W) == GLFW_PRESS)
    {
        MainCamera.EyeMoveBy(Dist * Forward);
    }

    if (glfwGetKey(Window, GLFW_KEY_S) == GLFW_PRESS)
    {
        MainCamera.EyeMoveBy(-Dist * Forward);
    }

    if (glfwGetKey(Window, GLFW_KEY_A) == GLFW_PRESS)
    {
        MainCamera.EyeMoveBy(-Dist * Right);
    }

    if (glfwGetKey(Window, GLFW_KEY_D) == GLFW_PRESS)
    {
        MainCamera.EyeMoveBy(Dist * Right);
    }

    if (glfwGetKey(Window, GLFW_KEY_E) == GLFW_PRESS)
    {
        MainCamera.EyeMoveBy(Dist * glm::vec3(0, 1, 0));
    }

    if (glfwGetKey(Window, GLFW_KEY_C) == GLFW_PRESS)
    {
        MainCamera.EyeMoveBy(Dist * glm::vec3(0, -1, 0));
    }
}

bool App::Init(int Argc, char** Argv)
{
    int ArgIndex = 1;
    while (ArgIndex < Argc)
    {
        std::string Arg = Argv[ArgIndex++];
        if (Arg == "-d")
        {
            DoApiDump = true;
        }
        else if (Arg == "-slang")
        {
            UseSlang = true;
        }
        else
        {
            fprintf(stderr, "Unknown argument: %s\n", Arg.c_str());
            return false;
        }
    }

    glfwSetErrorCallback(OnErrorCallback);

    if (!glfwInit())
    {
        fprintf(stderr, "Could not initialize GLFW.\n");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    Window = glfwCreateWindow(kWidth, kHeight, kProject, nullptr, nullptr);
    if (Window == nullptr)
    {
        fprintf(stderr, "Could not create the window.\n");
        return false;
    }

    if (!glfwVulkanSupported())
    {
        fprintf(stderr, "GLFW: Vulkan Not Supported\n");
        return false;
    }

    glfwSetWindowUserPointer(Window, this);
    glfwSetKeyCallback(Window, &KeyCallback);
    glfwSetCursorPosCallback(Window, &CursorPosCallback);
    glfwSetMouseButtonCallback(Window, &MouseButtonCallback);
    glfwSetFramebufferSizeCallback(Window, &FramebufferSizeCallback);
    return true;
}

}  // namespace rtrt

//----------------------------------------------------------------------------------------
int main(int Argc, char** Argv)
{
    rtrt::App Application;
    if (!Application.Init(Argc, Argv))
    {
        glfwTerminate();
        return 1;
    }

    rtrt::Renderer VK(&Application);
    if (!VK.Init())
    {
        glfwDestroyWindow(Application.GetWindow());
        glfwTerminate();
        return 1;
    }

    // The draw loop
    printf("looping =======================================\n");
    double FrameStart = glfwGetTime();
    while (!glfwWindowShouldClose(Application.GetWindow()))
    {
        glfwPollEvents();
        Application.UpdateCamera();

        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        Application.DrawGui(VK);

        const double CpuEnd = glfwGetTime();
        VK.DrawFrame();

        const double FrameEnd = glfwGetTime();
        Application.RecordFrameTimes(static_cast<float>((FrameEnd - FrameStart) * 1000.0),
                                     static_cast<float>((CpuEnd - FrameStart) * 1000.0),
                                     static_cast<float>(VK.GetRecordMs()),
                                     static_cast<float>(VK.GetGpuWaitMs()));
        FrameStart = FrameEnd;
    }

    // Cleanup

    VK.DestroyAllVulkanResources();

    glfwDestroyWindow(Application.GetWindow());
    glfwTerminate();
    return 0;
}
