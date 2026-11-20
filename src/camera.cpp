#include "camera.h"

#include <glm/gtx/transform.hpp>

namespace rtrt
{

glm::mat4 Camera::Perspective(float Aspect)
{
    glm::mat4 P = glm::mat4();

    float Rx = Ry * Aspect;
    P[0][0] = 1.0 / Rx;
    P[1][1] = -1.0 / Ry;  // Because Vulkan does y upside-down.
    // Becasue Vulkan wants [front,back] mapped to [0,1]
    P[2][2] = -Back / (Back - Front);
    P[3][2] = -(Front * Back) / (Back - Front);
    P[2][3] = -1;
    P[3][3] = 0;

    return P;
}

glm::mat4 Camera::View()
{
    glm::mat4 SpinMatrix = glm::rotate(glm::radians(Spin), glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 TiltMatrix = glm::rotate(glm::radians(Tilt), glm::vec3(1.0, 0.0, 0.0));
    glm::mat4 Translation = glm::translate(-Eye);
    return TiltMatrix * SpinMatrix * Translation;
}

void Camera::EyeMoveBy(const glm::vec3& Step)
{
    Eye += Step;
}

void Camera::MouseMove(float X, float Y)
{
    float Dx = X - PosX;
    float Dy = Y - PosY;
    Spin += Dx / 3;
    Tilt += Dy / 3;
    PosX = X;
    PosY = Y;
}

void Camera::SetMousePosition(float X, float Y)
{
    PosX = X;
    PosY = Y;
}

}  // namespace rtrt
