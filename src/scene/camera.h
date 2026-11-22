#ifndef RTRT_SRC_SCENE_CAMERA_H_
#define RTRT_SRC_SCENE_CAMERA_H_

#include <glm/glm.hpp>

namespace rtrt
{

// Fly-through camera: yaw (Spin) and pitch (Tilt) in degrees around an eye position.
class Camera
{
public:
    // Projection for this aspect ratio, with Vulkan's flipped y and [0,1] depth.
    glm::mat4 Perspective(float Aspect) const;

    // World-to-view matrix.
    glm::mat4 View() const;

    // Turns the camera by these angles, in degrees.
    void Rotate(float SpinBy, float TiltBy);

    void MoveBy(const glm::vec3& Step);

    // Restores the starting position and orientation.
    void Reset();

    const glm::vec3& GetEye() const
    {
        return Eye;
    }

    float GetSpin() const
    {
        return Spin;
    }

    float GetTilt() const
    {
        return Tilt;
    }

    // Vertical field of view, in degrees.
    float GetFovY() const;
    void SetFovY(float Degrees);

    void SetEye(const glm::vec3& Position)
    {
        Eye = Position;
    }

    // Sets yaw and pitch, in degrees.
    void SetAngles(float NewSpin, float NewTilt)
    {
        Spin = NewSpin;
        Tilt = NewTilt;
    }

private:
    float Ry = 0.57f;
    float Front = 0.1f;
    float Back = 1000.0f;

    float Spin = -20.0f;
    float Tilt = 10.66f;
    glm::vec3 Eye{2.29f, 1.68f, 6.64f};
};

}  // namespace rtrt

#endif  // RTRT_SRC_SCENE_CAMERA_H_
