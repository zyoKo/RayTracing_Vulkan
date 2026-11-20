#ifndef RTRT_SRC_CORE_WINDOW_H_
#define RTRT_SRC_CORE_WINDOW_H_

#include <functional>
#include <utility>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

namespace rtrt
{

// The GLFW window the swapchain presents to. Starts GLFW in Init and stops it when
// destroyed, and forwards key and mouse events to the handlers set on it.
class Window
{
public:
    using KeyHandler = std::function<void(int Key, int Action)>;
    using MouseButtonHandler =
        std::function<void(int Button, int Action, double X, double Y)>;
    using CursorPosHandler = std::function<void(double X, double Y)>;

    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Starts GLFW and opens a window with no client API. False when GLFW, the window or
    // Vulkan isn't available.
    bool Init(int Width, int Height, const char* Title);

    bool ShouldClose() const;
    void Close();
    void PollEvents();

    // Blocks while the window is minimized. False if it was closed meanwhile.
    bool WaitWhileMinimized();

    VkExtent2D GetFramebufferSize() const;
    bool IsKeyDown(int Key) const;
    bool IsMouseButtonDown(int Button) const;

    void SetKeyHandler(KeyHandler Handler)
    {
        OnKey = std::move(Handler);
    }

    void SetMouseButtonHandler(MouseButtonHandler Handler)
    {
        OnMouseButton = std::move(Handler);
    }

    void SetCursorPosHandler(CursorPosHandler Handler)
    {
        OnCursorPos = std::move(Handler);
    }

    GLFWwindow* GetHandle() const
    {
        return Handle;
    }

    // Set when the framebuffer changed size; the renderer clears it after recreating
    // the swapchain.
    bool GetFramebufferResized() const
    {
        return FramebufferResized;
    }

    void SetFramebufferResized(bool Resized)
    {
        FramebufferResized = Resized;
    }

private:
    static void FramebufferSizeCallback(GLFWwindow* Glfw, int Width, int Height);
    static void KeyCallback(GLFWwindow* Glfw, int Key, int Scancode, int Action,
                            int Mods);
    static void MouseButtonCallback(GLFWwindow* Glfw, int Button, int Action, int Mods);
    static void CursorPosCallback(GLFWwindow* Glfw, double X, double Y);

    GLFWwindow* Handle = nullptr;
    bool GlfwStarted = false;
    bool FramebufferResized = false;
    KeyHandler OnKey;
    MouseButtonHandler OnMouseButton;
    CursorPosHandler OnCursorPos;
};

}  // namespace rtrt

#endif  // RTRT_SRC_CORE_WINDOW_H_
