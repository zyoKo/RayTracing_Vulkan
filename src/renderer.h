#ifndef RTRT_SRC_RENDERER_H_
#define RTRT_SRC_RENDERER_H_

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "core/window.h"
#include "renderer/denoise_pass.h"
#include "renderer/graphics_pipeline.h"
#include "renderer/pass_context.h"
#include "renderer/post_pass.h"
#include "renderer/ray_trace_pass.h"
#include "renderer/render_image.h"
#include "renderer/scanline_pass.h"
#include "rt/acceleration_structure.h"
#include "rt/shader_binding_table.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "shaders/shared_structs.h"
#include "vk/buffer.h"
#include "vk/command_buffer.h"
#include "vk/command_pool.h"
#include "vk/context.h"
#include "vk/descriptor_builder.h"
#include "vk/descriptor_pool.h"
#include "vk/device.h"
#include "vk/fence.h"
#include "vk/framebuffer.h"
#include "vk/image.h"
#include "vk/pipeline.h"
#include "vk/pipeline_layout.h"
#include "vk/render_pass.h"
#include "vk/shader.h"
#include "vk/swapchain.h"
#include "vk_check.h"

namespace rtrt
{

// Settings the UI changes between frames.
struct RenderSettings
{
    bool UseRaytracer = true;
    int ViewMode = kViewFinal;
    bool DenoiseEnabled = true;
    int NumAtrousIterations = 5;
    float Exposure = 1.0f;
    float Gamma = 2.2f;
};

// Owns the Vulkan device and every resource used to draw the scene with the
// rasterizer or the path tracer, the denoiser and the final post pass.
class Renderer
{
public:
    Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Loads the scene and builds the swapchain, every pipeline and the ImGui backend.
    // The objects passed in must outlive the renderer.
    bool Init(Window& Target, Context& Instance, Device& Gpu, Swapchain& Chain,
              const Camera& View, bool UseSlangShaders);

    // Records, submits and presents one frame.
    void DrawFrame();

    void DestroyAllVulkanResources();

    // Puts every UI setting back to its starting value.
    void SetDefaultSettings();

    // Switches between the GLSL and Slang SPIR-V and rebuilds every pipeline.
    void SetShaderLanguage(bool UseSlangShaders);

    // Throws away the accumulated path-tracing history.
    void ResetRtAccumulation();

    RenderSettings& GetSettings()
    {
        return Settings;
    }

    PushConstantRay& GetRayConstants()
    {
        return RayTrace.GetConstants();
    }

    PushConstantDenoise& GetDenoiseConstants()
    {
        return Denoiser.GetConstants();
    }

    // Frames rendered since the history was last reset.
    int GetFrameCount() const
    {
        return FrameCount;
    }

    bool GetUseSlang() const
    {
        return UseSlang;
    }

    // Time blocked on the previous frame's fence.
    double GetGpuWaitMs() const
    {
        return GpuWaitMs;
    }

    // Time to acquire, record, submit and present.
    double GetRecordMs() const
    {
        return RecordMs;
    }

private:
    Window* MainWindow = nullptr;
    Context* GpuContext = nullptr;
    Device* GpuDevice = nullptr;
    Swapchain* MainSwapchain = nullptr;
    const Camera* MainCamera = nullptr;
    PassContext PassCtx;
    PostPass Post;
    ScanlinePass Scanline;
    RayTracePass RayTrace;
    DenoisePass Denoiser;

    RenderSettings Settings;
    // Frames rendered since the history was last reset
    int FrameCount = 0;
    bool ResetHistory = false;

    bool UseSlang = false;

    double GpuWaitMs = 0.0;
    double RecordMs = 0.0;

    // False if the window closed while minimized
    bool RecreateSizedResources();
    // Everything that depends on the window size.
    void DestroySizedResources();

    // Vulkan objects that will be created and the functions that will do the creation.

    CommandPool MainCmdPool;
    CommandBuffer FrameCmd;
    void CreateCommandPool();

    Fence FrameFence;
    void CreateSwapchain();
    void CreateFrameFence();

    Image DepthImage;
    void CreateDepthResource();

    DescriptorPool ImguiDescPool;
    void InitGui();

    Image ScImageBuffer{};
    void CreateScBuffer();

    // Each destroys one part of the renderer during shutdown.
    void DestroyPostResources();
    void DestroySceneResources();
    void DestroyScanlineResources();
    void DestroyRaytracingResources();
    void DestroyDenoiseResources();

    Scene MainScene;

    DescriptorBundle ScDesc;
    void CreateScDescriptorSet();

    Buffer MatrixBw{};  // Device-Host of the camera matrices
    void CreateMatrixBuffer();

    AccelerationStructure SceneAccel;
    void InitRayTracing();

    BlasInput ObjectToVkGeometryKhr(const Mesh& Model);
    void CreateRtAccelerationStructure();

    // Run loop
    bool PrepareFrame();

    glm::mat4 PriorViewProj{};
    void UpdateCameraBuffer();

    void SubmitFrame();
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_H_
