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
    bool modified;
    float ry;
    float front;
    float back;

    float spin;
    float tilt;
    glm::vec3 eye;

    float startSpin;
    float startTilt;
    glm::vec3 startEye;
    float startTime;
    float endTime;

    bool lmb = false;
    bool mmb = false;
    bool rmb = false;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;

    float posx = 0.0;
    float posy = 0.0;

    Camera();
    void animateTo(float deltaTime, float spin, float tilt, const glm::vec3& eye);
    glm::mat4 perspective(const float aspect);
    glm::mat4 view(float time);

    void mouseMove(const float x, const float y);
    void eyeMoveBy(const glm::vec3& step);
    void setMousePosition(const float x, const float y);
    void wheel(const int dir);
    void viewParms();
};

}  // namespace rtrt

#endif  // RTRT_SRC_CAMERA_H_
