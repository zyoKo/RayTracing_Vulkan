#ifndef RTRT_SRC_CAMERA_H_
#define RTRT_SRC_CAMERA_H_

#include <glm/glm.hpp>

namespace rtrt
{

class Camera
{
public:
    float ry = 0.57f;
    float front = 0.1f;
    float back = 1000.0f;

    float spin = -20.0f;
    float tilt = 10.66f;
    glm::vec3 eye{2.29f, 1.68f, 6.64f};

    bool lmb = false;

    float posx = 0.0f;
    float posy = 0.0f;

    glm::mat4 perspective(float aspect);
    glm::mat4 view();

    void mouseMove(float x, float y);
    void eyeMoveBy(const glm::vec3& step);
    void setMousePosition(float x, float y);
};

}  // namespace rtrt

#endif  // RTRT_SRC_CAMERA_H_
