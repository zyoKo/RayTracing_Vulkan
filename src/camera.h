#ifndef RTRT_SRC_CAMERA_H_
#define RTRT_SRC_CAMERA_H_

#include <glm/glm.hpp>

namespace rtrt
{

class Camera
{
public:
    float Ry = 0.57f;
    float Front = 0.1f;
    float Back = 1000.0f;

    float Spin = -20.0f;
    float Tilt = 10.66f;
    glm::vec3 Eye{2.29f, 1.68f, 6.64f};

    bool Lmb = false;

    float PosX = 0.0f;
    float PosY = 0.0f;

    glm::mat4 Perspective(float Aspect);
    glm::mat4 View();

    void MouseMove(float X, float Y);
    void EyeMoveBy(const glm::vec3& Step);
    void SetMousePosition(float X, float Y);
};

}  // namespace rtrt

#endif  // RTRT_SRC_CAMERA_H_
