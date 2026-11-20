#ifndef RTRT_SRC_CAMERA_H_
#define RTRT_SRC_CAMERA_H_

#include <glm/glm.hpp>

namespace rtrt
{

struct InputState
{
    bool lmb = false;
    bool mmb = false;
    bool rmb = false;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

class Camera
{
public:
    bool modified = false;
    float ry = 0.57f;
    float front = 0.1f;
    float back = 1000.0f;

    float spin = -20.0f;
    float tilt = 10.66f;
    glm::vec3 eye{2.29f, 1.68f, 6.64f};

    float startSpin = 0.0f;
    float startTilt = 0.0f;
    glm::vec3 startEye{0.0f};
    float startTime = 0.0f;
    float endTime = 0.0f;

    bool lmb = false;
    bool mmb = false;
    bool rmb = false;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;

    float posx = 0.0f;
    float posy = 0.0f;

    Camera();
    void animateTo(float deltaTime, float spin, float tilt, const glm::vec3& eye);
    glm::mat4 perspective(float aspect);
    glm::mat4 view(float time);

    void mouseMove(float x, float y);
    void eyeMoveBy(const glm::vec3& step);
    void setMousePosition(float x, float y);
    void wheel(int dir);
    void viewParms();
};

}  // namespace rtrt

#endif  // RTRT_SRC_CAMERA_H_
