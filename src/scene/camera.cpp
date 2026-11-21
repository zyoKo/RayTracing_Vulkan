#include "scene/camera.h"

#include <glm/gtx/transform.hpp>

namespace rtrt
{

glm::mat4 Camera::Perspective(float Aspect) const
{
    glm::mat4 P = glm::mat4();

    float Rx = Ry * Aspect;
    P[0][0] = 1.0f / Rx;
    P[1][1] = -1.0f / Ry;  // Because Vulkan does y upside-down.
    // Becasue Vulkan wants [front,back] mapped to [0,1]
    P[2][2] = -Back / (Back - Front);
    P[3][2] = -(Front * Back) / (Back - Front);
    P[2][3] = -1;
    P[3][3] = 0;

    return P;
}

glm::mat4 Camera::View() const
{
    glm::mat4 SpinMatrix = glm::rotate(glm::radians(Spin), glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 TiltMatrix = glm::rotate(glm::radians(Tilt), glm::vec3(1.0, 0.0, 0.0));
    glm::mat4 Translation = glm::translate(-Eye);
    return TiltMatrix * SpinMatrix * Translation;
}

void Camera::Rotate(float SpinBy, float TiltBy)
{
    Spin += SpinBy;
    Tilt += TiltBy;
}

void Camera::MoveBy(const glm::vec3& Step)
{
    Eye += Step;
}

void Camera::Reset()
{
    const Camera Defaults;
    Spin = Defaults.Spin;
    Tilt = Defaults.Tilt;
    Eye = Defaults.Eye;
}

}  // namespace rtrt
