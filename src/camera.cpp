#include "camera.h"

#include <GLFW/glfw3.h>
#include <glm/gtx/transform.hpp>

namespace rtrt
{

Camera::Camera()
{
    viewParms();
}

void Camera::animateTo(float deltaTime, float endSpin, float endTilt,
                       const glm::vec3& endEye)
{
    startTime = glfwGetTime();
    endTime = startTime + deltaTime;
    startSpin = spin;
    spin = endSpin;
    startTilt = tilt;
    tilt = endTilt;
    startEye = eye;
    eye = endEye;
}

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

glm::mat4 Camera::view(float time)
{
    float currSpin = spin;
    float currTilt = tilt;
    glm::vec3 currEye = eye;

    if (time < endTime)
    {
        modified = true;
        float t = (time - startTime) / (endTime - startTime);
        t = (3 - 2 * t) * t * t;
        currSpin = (1 - t) * startSpin + t * spin;
        currTilt = (1 - t) * startTilt + t * tilt;
        currEye = (1 - t) * startEye + t * eye;
    }

    glm::mat4 SPIN = glm::rotate(glm::radians(currSpin), glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 TILT = glm::rotate(glm::radians(currTilt), glm::vec3(1.0, 0.0, 0.0));
    glm::mat4 TRAN = glm::translate(-currEye);
    return TILT * SPIN * TRAN;
}

void Camera::eyeMoveBy(const glm::vec3& step)
{
    eye += step;
    modified = true;
    viewParms();
}

void Camera::mouseMove(float x, float y)
{
    float dx = x - posx;
    float dy = y - posy;
    spin += dx / 3;
    tilt += dy / 3;
    posx = x;
    posy = y;
    modified = true;
    viewParms();
}

void Camera::setMousePosition(float x, float y)
{
    posx = x;
    posy = y;
}

void Camera::wheel(int dir)
{
}

void Camera::viewParms()
{
}

}  // namespace rtrt
