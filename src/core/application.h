#ifndef RTRT_SRC_CORE_APPLICATION_H_
#define RTRT_SRC_CORE_APPLICATION_H_

#include "core/window.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "scene/camera_controller.h"
#include "ui/gui.h"
#include "vk/context.h"
#include "vk/device.h"
#include "vk/swapchain.h"

namespace rtrt
{

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

private:
    void OnKey(int Key, int Action);
    void OnMouseButton(double X, double Y);
    void OnCursorPos(double X, double Y);

    Window MainWindow;
    Context GpuContext;
    Device GpuDevice;
    Swapchain MainSwapchain;
    bool DoApiDump = false;
    // -slang: start with the Slang shaders
    bool UseSlang = false;

    Camera MainCamera;
    CameraController MainController;
    Renderer MainRenderer;
    Gui MainGui;
};

}  // namespace rtrt

#endif  // RTRT_SRC_CORE_APPLICATION_H_
