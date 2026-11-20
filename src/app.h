#ifndef RTRT_SRC_APP_H_
#define RTRT_SRC_APP_H_

#include <algorithm>
#include <cfloat>

#include <GLFW/glfw3.h>

#include "camera.h"

namespace rtrt
{

struct FrameStats
{
    static constexpr int kFrameCount = 240;
    float FrameMs[kFrameCount] = {};
    float CpuMs[kFrameCount] = {};
    float DrawMs[kFrameCount] = {};
    float GpuWaitMs[kFrameCount] = {};
    int Next = 0;
    int Count = 0;

    void Push(float Frame, float Cpu, float Draw, float GpuWait)
    {
        FrameMs[Next] = Frame;
        CpuMs[Next] = Cpu;
        DrawMs[Next] = Draw;
        GpuWaitMs[Next] = GpuWait;
        Next = (Next + 1) % kFrameCount;
        Count = std::min(Count + 1, kFrameCount);
    }

    void Range(const float* Values, float& Avg, float& Lo, float& Hi) const
    {
        Avg = 0.0f;
        Lo = FLT_MAX;
        Hi = 0.0f;
        for (int I = 0; I < Count; I++)
        {
            Avg += Values[I];
            Lo = std::min(Lo, Values[I]);
            Hi = std::max(Hi, Values[I]);
        }
        Avg = Count ? Avg / Count : 0.0f;
        if (!Count)
        {
            Lo = 0.0f;
        }
    }
};

class App
{
public:
    App() = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    bool Init(int Argc, char** Argv);

    GLFWwindow* Window = nullptr;
    bool DoApiDump = false;
    bool UseSlang = false;  // -slang: start with the Slang shaders

    bool ShowGui = true;
    bool ShowPerf = false;
    bool FramebufferResized = false;
    float MoveSpeed = 0.7f;
    Camera MainCamera;
    FrameStats Stats;
    float LastTime = 0;
    void UpdateCamera();
};

}  // namespace rtrt

#endif  // RTRT_SRC_APP_H_
