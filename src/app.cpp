#include <iostream>
#include <array>

#include "vkapp.h"
#include "app.h"
#include "extensions_vk.hpp"

// GLFW Callback functions
static void onErrorCallback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}


void drawGUI(VkApp& VK)
{
    App& A = *VK.app;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Settings (F1 to hide)", &A.m_show_gui)) {
        ImGui::End();
        return; }
    ImGui::PushItemWidth(150);

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("%.1f FPS (%.2f ms)", io.Framerate, 1000.0f/io.Framerate);
    ImGui::Text("Frames since reset: %d", VK.frameCount);

    // Anything that changes what a path returns makes the existing history wrong.
    bool resetHistory = false;

    if (ImGui::RadioButton("Ray tracer", VK.useRaytracer)) {
        resetHistory |= !VK.useRaytracer;
        VK.useRaytracer = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rasterizer", !VK.useRaytracer))
        VK.useRaytracer = false;

    const char* viewNames[] = {"Final", "Albedo", "Normal", "Depth", "Sample count"};
    ImGui::Combo("View", &VK.m_viewMode, viewNames, IM_ARRAYSIZE(viewNames));

    if (ImGui::CollapsingHeader("Path tracer", ImGuiTreeNodeFlags_DefaultOpen)) {
        resetHistory |= ImGui::Checkbox("Specular", &VK.m_pcRay.fullBRDF);
        resetHistory |= ImGui::SliderFloat("Survival chance", &VK.m_pcRay.rr, 0.1f, 0.95f, "%.2f");
        resetHistory |= ImGui::SliderInt("Max path length", &VK.m_pcRay.depth, 1, 32);
        resetHistory |= ImGui::SliderFloat("Emission scale", &VK.m_pcRay.emissionScale, 0.0f, 5.0f, "%.2f"); }

    if (ImGui::CollapsingHeader("Temporal history", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Use history", &VK.m_pcRay.useHistory);
        ImGui::SliderFloat("Normal threshold", &VK.m_pcRay.n_threshold, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Depth threshold", &VK.m_pcRay.d_threshold, 0.001f, 0.2f, "%.3f");
        ImGui::SliderFloat("Max samples (0 = off)", &VK.m_pcRay.maxSamples, 0.0f, 4096.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
        if (ImGui::Button("Reset history"))
            resetHistory = true; }

    if (ImGui::CollapsingHeader("Denoiser", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enabled", &VK.m_denoiseEnabled);
        ImGui::SliderInt("Iterations", &VK.m_num_atrous_iterations, 0, 8);
        ImGui::SliderFloat("Normal factor", &VK.m_pcDenoise.normFactor, 0.0f, 0.1f, "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Depth factor", &VK.m_pcDenoise.depthFactor, 0.0f, 0.1f, "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Albedo factor", &VK.m_pcDenoise.albedoFactor, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic); }

    if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Exposure", &VK.m_exposure, 0.1f, 4.0f, "%.2f");
        ImGui::SliderFloat("Gamma", &VK.m_gamma, 1.0f, 3.0f, "%.2f"); }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Position: %.2f, %.2f, %.2f", A.myCamera.eye.x, A.myCamera.eye.y, A.myCamera.eye.z);
        ImGui::SliderFloat("Move speed", &A.m_moveSpeed, 0.1f, 5.0f, "%.2f");
        if (ImGui::Button("Reset camera")) {
            const Camera start;
            A.myCamera.spin = start.spin;
            A.myCamera.tilt = start.tilt;
            A.myCamera.eye  = start.eye; } }

    ImGui::Separator();
    if (ImGui::Button("Restore default settings"))
        VK.setDefaultSettings();

    if (resetHistory)
        VK.ResetRtAccumulation();

    ImGui::PopItemWidth();
    ImGui::End();
}

//////////////////////////////////////////////////////////////////////////
static int const WIDTH  = 10*128;
static int const HEIGHT = 6*128;
static std::string PROJECT = "rtrt";

App* app;  // The app, declared here so static callback functions can find it.

//---------------------------------------------------------------------------------------------
int main(int argc, char** argv)
{
    app =  new App(argc, argv); // Constructs the glfw window and sets UI callbacks

    
    VkApp VK(app); // Creates and manages all things Vulkan.

    // The draw loop
    printf("looping =======================================\n");
    while(!glfwWindowShouldClose(app->GLFW_window)) {
        glfwPollEvents();
        app->updateCamera();
        
        #ifdef GUI
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if(app->m_show_gui)
            drawGUI(VK);
        #endif

        VK.drawFrame();
    }

    // Cleanup

    VK.destroyAllVulkanResources();

    glfwDestroyWindow(app->GLFW_window);
    glfwTerminate();
}

void framebuffersize_cb(GLFWwindow* window, int w, int h)
{
    assert(false && "Not ready for window resize events.");
}

void scroll_cb(GLFWwindow* window, double x, double y)
{
    #ifdef GUI
    if(ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
        return;
    #endif

    int delta = y;
    if(delta != 0)
        app->myCamera.wheel(delta > 0 ? 1 : -1);
}

void mousebutton_cb(GLFWwindow* window, int button, int action, int mods)
{
    #ifdef GUI
    if(ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
        return;
    #endif

    double x, y;
    glfwGetCursorPos(window, &x, &y);
    app->myCamera.setMousePosition(x, y);
}

void cursorpos_cb(GLFWwindow* window, double x, double y)
{
    #ifdef GUI
    if(ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
        return;
    #endif

    if (app->myCamera.lmb || app->myCamera.rmb || app->myCamera.mmb)
        app->myCamera.mouseMove(x, y);
}

void char_cb(GLFWwindow* window, unsigned int key)
{
    #ifdef GUI
    if(ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard)
        return;
    #endif
}

void key_cb(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    const bool pressed = action != GLFW_RELEASE;

    // Keys typed into a settings field (Ctrl+click on a slider) belong to that field.
    #ifdef GUI
    if(ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput)
        return;
    #endif

    if (pressed && key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, 1);

    if (action == GLFW_PRESS && key == GLFW_KEY_F1)
        app->m_show_gui = !app->m_show_gui;
}

static float lastTime = 0;

void App::updateCamera()
{
    float now = glfwGetTime();
    float dt = now-lastTime;
    lastTime = now;
    
    float dist = m_moveSpeed*dt;

    float rad = 3.14159/180;

    myCamera.lmb   = glfwGetMouseButton(GLFW_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // Letters typed into a settings field (Ctrl+click on a slider) must not move the camera.
    #ifdef GUI
    if(ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput)
        return;
    #endif

    if (glfwGetKey(GLFW_window, GLFW_KEY_W) == GLFW_PRESS) {
        myCamera.eyeMoveBy(dist*glm::vec3(sin(myCamera.spin*rad), 0.0, -cos(myCamera.spin*rad))); }
    
    if (glfwGetKey(GLFW_window, GLFW_KEY_S) == GLFW_PRESS) {
        myCamera.eyeMoveBy(-dist*glm::vec3(sin(myCamera.spin*rad), 0.0, -cos(myCamera.spin*rad))); }
    
    if (glfwGetKey(GLFW_window, GLFW_KEY_A) == GLFW_PRESS) {
        myCamera.eyeMoveBy(-dist*glm::vec3(cos(myCamera.spin*rad), 0.0, sin(myCamera.spin*rad)));; }
    
    if (glfwGetKey(GLFW_window, GLFW_KEY_D) == GLFW_PRESS) {
        myCamera.eyeMoveBy(dist*glm::vec3(cos(myCamera.spin*rad), 0.0, sin(myCamera.spin*rad))); }
    
    if (glfwGetKey(GLFW_window, GLFW_KEY_E) == GLFW_PRESS) {
        myCamera.eyeMoveBy(dist*glm::vec3(0,1,0)); }
        
    if (glfwGetKey(GLFW_window, GLFW_KEY_C) == GLFW_PRESS) {
        myCamera.eyeMoveBy(dist*glm::vec3(0,-1,0)); }
}

App::App(int argc, char** argv)
{
    doApiDump = false;

    int argi = 1;
    while (argi<argc) {
        std::string arg = argv[argi++];
        if (arg == "-d")
            doApiDump = true;
        else {
            printf("Unknown argument: %s\n", arg.c_str());
            exit(-1); } }

    glfwSetErrorCallback(onErrorCallback);

    if(!glfwInit()) {
        printf("Could not initialize GLFW.");
        exit(1); }
  
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFW_window = glfwCreateWindow(WIDTH, HEIGHT, PROJECT.c_str(), nullptr, nullptr);

    if(!glfwVulkanSupported()) {
        printf("GLFW: Vulkan Not Supported\n");
        exit(1); }

    glfwSetWindowUserPointer(GLFW_window, this);
    glfwSetKeyCallback(GLFW_window, &key_cb);
    glfwSetCharCallback(GLFW_window, &char_cb);
    glfwSetCursorPosCallback(GLFW_window, &cursorpos_cb);
    glfwSetMouseButtonCallback(GLFW_window, &mousebutton_cb);
    glfwSetScrollCallback(GLFW_window, &scroll_cb);
    glfwSetFramebufferSizeCallback(GLFW_window, &framebuffersize_cb);
}
