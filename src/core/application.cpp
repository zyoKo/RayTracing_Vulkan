#include "core/application.h"

#include <cstdio>
#include <string>

#include <GLFW/glfw3.h>

#include "core/shader_check.h"
#include "renderer/renderer.h"

namespace rtrt
{

namespace
{

constexpr int kWidth = 10 * 128;
constexpr int kHeight = 6 * 128;
constexpr char kProject[] = "rtrt";
// Frames to accumulate before a -shot capture
constexpr int kShotFrame = 240;

}  // namespace

void Application::OnMouseButton(double X, double Y)
{
    if (Gui::WantsMouse())
    {
        return;
    }
    MainController.OnMouseButton(static_cast<float>(X), static_cast<float>(Y));
}

void Application::OnCursorPos(double X, double Y)
{
    if (Gui::WantsMouse())
    {
        return;
    }
    MainController.OnCursorPos(static_cast<float>(X), static_cast<float>(Y), MainCamera);
}

void Application::OnKey(int Key, int Action)
{
    // Keys typed into a settings field (Ctrl+click on a slider) belong to that field.
    if (Gui::WantsTextInput())
    {
        return;
    }

    if (Action != GLFW_RELEASE && Key == GLFW_KEY_ESCAPE)
    {
        MainWindow.Close();
    }

    if (Action == GLFW_PRESS && Key == GLFW_KEY_F1)
    {
        MainGui.ToggleSettings();
    }
}

void Application::UpdateCamera()
{
    // Letters typed into a settings field (Ctrl+click on a slider) must not move the
    // camera.
    MainController.Update(MainWindow, !Gui::WantsTextInput(), MainCamera);
}

bool Application::Init(int Argc, char** Argv)
{
    int ArgIndex = 1;
    while (ArgIndex < Argc)
    {
        std::string Arg = Argv[ArgIndex++];
        if (Arg == "-d")
        {
            DoApiDump = true;
        }
        else if (Arg == "-glsl")
        {
            UseSlang = false;
        }
        else if (Arg == "-shot" && ArgIndex < Argc)
        {
            ShotPath = Argv[ArgIndex++];
        }
        else
        {
            fprintf(stderr, "Unknown argument: %s\n", Arg.c_str());
            return false;
        }
    }

    if (!EnsureShaders(!UseSlang))
    {
        return false;
    }

    if (!MainWindow.Init(kWidth, kHeight, kProject))
    {
        return false;
    }

    MainWindow.SetKeyHandler([this](int Key, int Action) { OnKey(Key, Action); });
    MainWindow.SetMouseButtonHandler([this](int, int, double X, double Y)
                                     { OnMouseButton(X, Y); });
    MainWindow.SetCursorPosHandler([this](double X, double Y) { OnCursorPos(X, Y); });

    if (!GpuContext.Init(MainWindow, DoApiDump) || !GpuDevice.Init(GpuContext) ||
        !MainRenderer.Init(MainWindow, GpuContext, GpuDevice, MainSwapchain, MainCamera,
                           UseSlang))
    {
        return false;
    }
    MainGui.Init(MainWindow, GpuContext, GpuDevice, MainRenderer.GetUiRenderPass(),
                 MainSwapchain.GetImageCount());
    return true;
}

int Application::Run()
{
    printf("looping =======================================\n");
    double FrameStart = glfwGetTime();
    while (!MainWindow.ShouldClose())
    {
        MainWindow.PollEvents();
        UpdateCamera();

        MainRenderer.ApplyRenderExtent();
        if (!ShotPath.empty() && ++FramesDrawn == kShotFrame)
        {
            MainRenderer.RequestScreenshot(ShotPath);
        }
        MainGui.Draw(MainRenderer, MainCamera, MainController);

        const double CpuEnd = glfwGetTime();
        MainRenderer.DrawFrame();

        const double FrameEnd = glfwGetTime();
        MainGui.RecordFrameTimes(static_cast<float>((FrameEnd - FrameStart) * 1000.0),
                                 static_cast<float>((CpuEnd - FrameStart) * 1000.0),
                                 static_cast<float>(MainRenderer.GetRecordMs()),
                                 static_cast<float>(MainRenderer.GetGpuWaitMs()));
        FrameStart = FrameEnd;
    }

    MainGui.Shutdown();
    MainRenderer.DestroyAllVulkanResources();
    return 0;
}

}  // namespace rtrt
