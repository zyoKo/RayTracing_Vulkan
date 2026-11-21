#ifndef RTRT_SRC_SCENE_CAMERA_CONTROLLER_H_
#define RTRT_SRC_SCENE_CAMERA_CONTROLLER_H_

#include "core/window.h"
#include "scene/camera.h"

namespace rtrt
{

// Flies a Camera with WASD, E and C, and turns it while the left mouse button is held.
class CameraController
{
public:
    // Remembers where the cursor was when a mouse button changed.
    void OnMouseButton(float X, float Y);

    // Turns Target by the cursor movement while the left button is held.
    void OnCursorPos(float X, float Y, Camera& Target);

    // Moves Target for the keys held since the last call. With ReadKeys false only the
    // clock and the mouse button are updated.
    void Update(const Window& Input, bool ReadKeys, Camera& Target);

    float GetMoveSpeed() const
    {
        return MoveSpeed;
    }

    void SetMoveSpeed(float Speed)
    {
        MoveSpeed = Speed;
    }

private:
    float MoveSpeed = 0.7f;
    bool LeftButtonDown = false;
    float LastX = 0.0f;
    float LastY = 0.0f;
    double LastTime = 0.0;
};

}  // namespace rtrt

#endif  // RTRT_SRC_SCENE_CAMERA_CONTROLLER_H_
