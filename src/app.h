
#include "camera.h"

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
    void updateCamera();
};
