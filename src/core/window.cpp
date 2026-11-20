#include "core/window.h"

#include <cstdint>
#include <cstdio>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

namespace rtrt
{

namespace
{

void OnErrorCallback(int Error, const char* Description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", Error, Description);
}

Window* FromGlfw(GLFWwindow* Glfw)
{
    return static_cast<Window*>(glfwGetWindowUserPointer(Glfw));
}

}  // namespace

Window::~Window()
{
    if (Handle != nullptr)
    {
        glfwDestroyWindow(Handle);
    }
    if (GlfwStarted)
    {
        glfwTerminate();
    }
}

bool Window::Init(int Width, int Height, const char* Title)
{
    glfwSetErrorCallback(OnErrorCallback);

    if (!glfwInit())
    {
        fprintf(stderr, "Could not initialize GLFW.\n");
        return false;
    }
    GlfwStarted = true;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    Handle = glfwCreateWindow(Width, Height, Title, nullptr, nullptr);
    if (Handle == nullptr)
    {
        fprintf(stderr, "Could not create the window.\n");
        return false;
    }

    if (!glfwVulkanSupported())
    {
        fprintf(stderr, "GLFW: Vulkan Not Supported\n");
        return false;
    }

    glfwSetWindowUserPointer(Handle, this);
    glfwSetKeyCallback(Handle, &KeyCallback);
    glfwSetCursorPosCallback(Handle, &CursorPosCallback);
    glfwSetMouseButtonCallback(Handle, &MouseButtonCallback);
    glfwSetFramebufferSizeCallback(Handle, &FramebufferSizeCallback);
    return true;
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(Handle);
}

void Window::Close()
{
    glfwSetWindowShouldClose(Handle, 1);
}

void Window::PollEvents()
{
    glfwPollEvents();
}

bool Window::WaitWhileMinimized()
{
    // A minimized window has a zero-sized framebuffer, which can't back a swapchain.
    VkExtent2D Size = GetFramebufferSize();
    while (Size.width == 0 || Size.height == 0)
    {
        if (ShouldClose())
        {
            return false;
        }
        glfwWaitEvents();
        Size = GetFramebufferSize();
    }
    return true;
}

VkExtent2D Window::GetFramebufferSize() const
{
    int Width = 0;
    int Height = 0;
    glfwGetFramebufferSize(Handle, &Width, &Height);
    return VkExtent2D{static_cast<uint32_t>(Width), static_cast<uint32_t>(Height)};
}

bool Window::IsKeyDown(int Key) const
{
    return glfwGetKey(Handle, Key) == GLFW_PRESS;
}

bool Window::IsMouseButtonDown(int Button) const
{
    return glfwGetMouseButton(Handle, Button) == GLFW_PRESS;
}

void Window::FramebufferSizeCallback(GLFWwindow* Glfw, int, int)
{
    FromGlfw(Glfw)->FramebufferResized = true;
}

void Window::KeyCallback(GLFWwindow* Glfw, int Key, int, int Action, int)
{
    Window* Self = FromGlfw(Glfw);
    if (Self->OnKey)
    {
        Self->OnKey(Key, Action);
    }
}

void Window::MouseButtonCallback(GLFWwindow* Glfw, int Button, int Action, int)
{
    Window* Self = FromGlfw(Glfw);
    if (Self->OnMouseButton)
    {
        double X = 0.0;
        double Y = 0.0;
        glfwGetCursorPos(Glfw, &X, &Y);
        Self->OnMouseButton(Button, Action, X, Y);
    }
}

void Window::CursorPosCallback(GLFWwindow* Glfw, double X, double Y)
{
    Window* Self = FromGlfw(Glfw);
    if (Self->OnCursorPos)
    {
        Self->OnCursorPos(X, Y);
    }
}

}  // namespace rtrt
