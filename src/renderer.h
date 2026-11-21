#ifndef RTRT_SRC_RENDERER_H_
#define RTRT_SRC_RENDERER_H_

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "core/window.h"
#include "rt/acceleration_structure.h"
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
        return PcRay;
    }

    PushConstantDenoise& GetDenoiseConstants()
    {
        return PcDenoise;
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
    void WriteSizedDescriptors();

    // Vulkan objects that will be created and the functions that will do the creation.

    CommandPool MainCmdPool;
    CommandBuffer FrameCmd;
    void CreateCommandPool();

    Fence FrameFence;
    void CreateSwapchain();
    void CreateFrameFence();

    Image DepthImage;
    void CreateDepthResource();

    PipelineLayout PostPipelineLayout;
    RenderPass PostRenderPass;
    void CreatePostRenderPass();

    // One per swapchain image
    std::vector<Framebuffer> Framebuffers;
    void CreatePostFrameBuffers();

    Pipeline PostPipeline;
    void CreatePostPipeline();

    // GLSL and Slang SPIR-V files for a vertex and a fragment shader.
    struct GraphicsShaders
    {
        const char* VertGlsl;
        const char* VertSlang;
        const char* FragGlsl;
        const char* FragSlang;
    };

    // The fixed-function state the scanline and post pipelines share.
    struct FixedFunctionState
    {
        VkPipelineInputAssemblyStateCreateInfo InputAssembly{};
        VkViewport Viewport{};
        VkRect2D Scissor{};
        VkPipelineViewportStateCreateInfo ViewportState{};
        VkPipelineRasterizationStateCreateInfo Rasterizer{};
        VkPipelineMultisampleStateCreateInfo Multisampling{};
        VkPipelineDepthStencilStateCreateInfo DepthStencil{};
        VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
        VkPipelineColorBlendStateCreateInfo ColorBlending{};
    };

    // A full-window triangle-list pipeline with depth test and no blending.
    Pipeline CreateGraphicsPipeline(
        const GraphicsShaders& Shaders,
        const VkPipelineVertexInputStateCreateInfo& VertexInput, VkPipelineLayout Layout,
        VkRenderPass Pass);
    FixedFunctionState MakeFixedFunctionState() const;

    DescriptorPool ImguiDescPool;
    void InitGui();

    RenderPass ScanlineRenderPass;
    Framebuffer ScanlineFramebuffer;
    void CreateScanlineRenderPass();
    void CreateScanlineFramebuffer();

    Image ScImageBuffer{};
    void CreateScBuffer();

    // Each destroys one part of the renderer during shutdown.
    void DestroyPostResources();
    void DestroySceneResources();
    void DestroyScanlineResources();
    void DestroyRaytracingResources();
    void DestroyDenoiseResources();

    Image RtColCurrBuffer{};
    Image RtColPrevBuffer{};

    Image RtKdCurrBuffer{};
    Image RtKdPrevBuffer{};

    Image RtNdCurrBuffer{};
    Image RtNdPrevBuffer{};

    void CreateRtBuffers();

    Image DenoiseBuffer{};
    void CreateDenoiseBuffer();

    Scene MainScene;

    DescriptorBundle ScDesc;
    void CreateScDescriptorSet();

    PipelineLayout ScanlinePipelineLayout;
    Pipeline ScanlinePipeline;
    void CreateScPipeline();

    Buffer MatrixBw{};  // Device-Host of the camera matrices
    void CreateMatrixBuffer();

    AccelerationStructure SceneAccel;
    float MaxAnis = 0;
    PushConstantRay PcRay{};  // Push constant for ray tracer
    PushConstantDenoise PcDenoise{};
    uint32_t HandleSize{};
    uint32_t HandleAlignment{};
    uint32_t BaseAlignment{};
    void InitRayTracing();

    BlasInput ObjectToVkGeometryKhr(const Mesh& Model);
    void CreateRtAccelerationStructure();

    DescriptorBundle RtDesc;
    void CreateRtDescriptorSet();

    PipelineLayout RtPipelineLayout;
    Pipeline RtPipeline;
    void CreateRtPipeline();
    // One raygen, one miss and one closest-hit shader, each in its own group.
    void CollectRtShaders(std::vector<Shader>& Modules,
                          std::vector<VkPipelineShaderStageCreateInfo>& Stages,
                          std::vector<VkRayTracingShaderGroupCreateInfoKHR>& Groups);
    void CreateRtPipelineLayout();

    Buffer ShaderBindingTableBw;
    VkStridedDeviceAddressRegionKHR RgenRegion{};
    VkStridedDeviceAddressRegionKHR MissRegion{};
    VkStridedDeviceAddressRegionKHR HitRegion{};
    VkStridedDeviceAddressRegionKHR CallRegion{};
    void CreateRtShaderBindingTable();
    // Fills the stride and size of each region from the device's handle alignment.
    void ComputeSbtRegions(uint32_t MissCount, uint32_t HitCount);
    // Copies the raygen, miss and hit handles into the mapped staging buffer.
    void WriteSbtHandles(const Buffer& Staging, VkDeviceSize SbtSize,
                         const std::vector<uint8_t>& Handles, uint32_t MissCount,
                         uint32_t HitCount);

    DescriptorBundle PostDesc;
    void CreatePostDescriptor();

    DescriptorBundle DenoiseDesc;
    void CreateDenoiseDescriptorSet();

    PipelineLayout DenoiseCompPipelineLayout;
    Pipeline DenoisePipeline;
    void CreateDenoiseCompPipeline();

    void CmdCopyImage(Image& Src, Image& Dst);

    void ImageLayoutBarrier(VkCommandBuffer Cmdbuffer, VkImage TargetImage,
                            VkImageLayout OldImageLayout, VkImageLayout NewImageLayout,
                            VkImageAspectFlags AspectMask = VK_IMAGE_ASPECT_COLOR_BIT);
    // Run loop
    bool PrepareFrame();

    glm::mat4 PriorViewProj{};
    void UpdateCameraBuffer();
    void Rasterize();
    void Raytrace();
    void Denoise();

    void PostProcess();
    void SubmitFrame();

    void CopyBuffer(VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize Size);
    Buffer CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage,
                        VkMemoryPropertyFlags Properties);

    Image CreateBufferImage(VkExtent2D Size);

    Image CreateImage(uint32_t Width, uint32_t Height, VkFormat Format,
                      VkImageUsageFlags Usage, VkMemoryPropertyFlags Properties,
                      uint32_t MipLevels = 1);
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_H_
