#include "camera.h"

#include <glm/gtx/transform.hpp>

namespace rtrt
{

glm::mat4 Camera::perspective(float aspect)
{
    glm::mat4 P = glm::mat4();

    float rx = ry * aspect;
    P[0][0] = 1.0 / rx;
    P[1][1] = -1.0 / ry;  // Because Vulkan does y upside-down.
    // Becasue Vulkan wants [front,back] mapped to [0,1]
    P[2][2] = -back / (back - front);
    P[3][2] = -(front * back) / (back - front);
    P[2][3] = -1;
    P[3][3] = 0;

    return P;
}

glm::mat4 Camera::view()
{
    glm::mat4 SPIN = glm::rotate(glm::radians(spin), glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 TILT = glm::rotate(glm::radians(tilt), glm::vec3(1.0, 0.0, 0.0));
    glm::mat4 TRAN = glm::translate(-eye);
    return TILT * SPIN * TRAN;
}

void Camera::eyeMoveBy(const glm::vec3& step)
{
    eye += step;
}

void Camera::mouseMove(float x, float y)
{
    float dx = x - posx;
    float dy = y - posy;
    spin += dx / 3;
    tilt += dy / 3;
    posx = x;
    posy = y;
}

void Camera::setMousePosition(float x, float y)
{
    posx = x;
    posy = y;
}

}  // namespace rtrt
