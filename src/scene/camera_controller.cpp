#include "scene/camera_controller.h"

#include <cmath>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

namespace rtrt
{

void CameraController::OnMouseButton(float X, float Y)
{
    LastX = X;
    LastY = Y;
}

void CameraController::OnCursorPos(float X, float Y, Camera& Target)
{
    if (!LeftButtonDown)
    {
        return;
    }
    Target.Rotate((X - LastX) / 3, (Y - LastY) / 3);
    LastX = X;
    LastY = Y;
}

void CameraController::Update(const Window& Input, bool ReadKeys, Camera& Target)
{
    const double Now = glfwGetTime();
    const float Dist = MoveSpeed * static_cast<float>(Now - LastTime);
    LastTime = Now;

    LeftButtonDown = Input.IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);

    if (!ReadKeys)
    {
        return;
    }

    const float Spin = glm::radians(Target.GetSpin());
    const glm::vec3 Forward(std::sin(Spin), 0.0f, -std::cos(Spin));
    const glm::vec3 Right(std::cos(Spin), 0.0f, std::sin(Spin));
    const glm::vec3 Up(0.0f, 1.0f, 0.0f);

    if (Input.IsKeyDown(GLFW_KEY_W))
    {
        Target.MoveBy(Dist * Forward);
    }
    if (Input.IsKeyDown(GLFW_KEY_S))
    {
        Target.MoveBy(-Dist * Forward);
    }
    if (Input.IsKeyDown(GLFW_KEY_A))
    {
        Target.MoveBy(-Dist * Right);
    }
    if (Input.IsKeyDown(GLFW_KEY_D))
    {
        Target.MoveBy(Dist * Right);
    }
    if (Input.IsKeyDown(GLFW_KEY_E))
    {
        Target.MoveBy(Dist * Up);
    }
    if (Input.IsKeyDown(GLFW_KEY_C))
    {
        Target.MoveBy(-Dist * Up);
    }
}

}  // namespace rtrt
