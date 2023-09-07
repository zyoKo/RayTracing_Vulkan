
#include "vkapp.h"
#include <iostream>
#include "camera.h"

Camera::Camera() : spin(-20.0), tilt(10.66), ry(0.57), front(0.1), back(1000.0),
                   eye({2.29, 1.68, 6.64}), endTime(0)
{
    viewParms();
}

void Camera::animateTo(float deltaTime, float endSpin, float endTilt, const glm::vec3& endEye)
{
    startTime = glfwGetTime();
    endTime = startTime + deltaTime;
    startSpin = spin;  spin = endSpin;
    startTilt = tilt;  tilt = endTilt;
    startEye = eye;  eye = endEye;
}

glm::mat4 Camera::perspective(const float aspect)
{
    glm::mat4 P = glm::mat4();

    float rx = ry*aspect;
    P[0][0] = 1.0/rx;
    P[1][1] = -1.0/ry; // Because Vulkan does y upside-down.
    P[2][2] = -back/(back-front);  // Becasue Vulkan wants [front,back] mapped to [0,1]
    P[3][2] = -(front*back)/(back-front);
    P[2][3] = -1;
    P[3][3] = 0;

    return P;
}

glm::mat4 Camera::view(float time)
{
    float currSpin = spin;
    float currTilt = tilt;
    glm::vec3 currEye = eye;
    
    if (time < endTime) {
        modified = true;
        float t = (time-startTime)/(endTime-startTime);
        //t = 2*t-t*t;
        t = (3-2*t)*t*t;
        currSpin = (1-t)*startSpin + t*spin;
        currTilt = (1-t)*startTilt + t*tilt;
        currEye  = (1-t)*startEye  + t*eye;
    }
    
    glm::mat4 SPIN = glm::rotate(currSpin*3.14159f/180.0f, glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 TILT = glm::rotate(currTilt*3.14159f/180.0f, glm::vec3(1.0, 0.0, 0.0));
    glm::mat4 TRAN = glm::translate(-currEye);
    return TILT*SPIN*TRAN;
}

void Camera::eyeMoveBy(const glm::vec3& step)
{
    eye += step;
    modified = true;
    viewParms();
}

void Camera::mouseMove(const float x, const float y)
{
    float dx = x-posx;
    float dy = y-posy;
    spin += dx/3;
    tilt += dy/3;
    posx = x;
    posy = y;
    modified = true;
    viewParms();
}

void Camera::setMousePosition(const float x, const float y)
{
    posx = x;
    posy = y;
}

void Camera::wheel(const int dir)
{
    //printf("wheel: %d\n", dir);
}

void Camera::viewParms()
{
    //printf("%5.2f, %5.2f, glm::vec3(%5.2f,%5.2f,%5.2f)\n",
    //       spin, tilt, eye[0], eye[1], eye[2]);
}
