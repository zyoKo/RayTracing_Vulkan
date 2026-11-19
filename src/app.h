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
    static constexpr int N = 240;
    float frameMs[N] = {};
    float cpuMs[N] = {};
    float drawMs[N] = {};
    float gpuWaitMs[N] = {};
    int next = 0;
    int count = 0;

    void push(float frame, float cpu, float draw, float gpuWait)
    {
        frameMs[next] = frame;
        cpuMs[next] = cpu;
        drawMs[next] = draw;
        gpuWaitMs[next] = gpuWait;
        next = (next + 1) % N;
        count = std::min(count + 1, N);
    }

    void range(const float* v, float& avg, float& lo, float& hi) const
    {
        avg = 0.0f;
        lo = FLT_MAX;
        hi = 0.0f;
        for (int i = 0; i < count; i++)
        {
            avg += v[i];
            lo = std::min(lo, v[i]);
            hi = std::max(hi, v[i]);
        }
        avg = count ? avg / count : 0.0f;
        if (!count)
        {
            lo = 0.0f;
        }
    }
};

class App
{
public:
    GLFWwindow* GLFW_window;
    App(int argc, char** argv);
    bool doApiDump;
    bool useSlang;  // -slang: start with the Slang shaders

    bool m_show_gui = true;
    bool m_show_perf = false;
    bool m_framebufferResized = false;
    float m_moveSpeed = 0.7f;
    Camera myCamera;
    FrameStats m_frameStats;
    float m_lastTime = 0;
    void updateCamera();
};

}  // namespace rtrt

#endif  // RTRT_SRC_APP_H_
