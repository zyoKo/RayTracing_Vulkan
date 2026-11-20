#ifndef RTRT_SRC_RENDERER_H_
#define RTRT_SRC_RENDERER_H_

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "acceleration_wrap.h"
#include "buffer_wrap.h"
#include "descriptor_wrap.h"
#include "image_wrap.h"
#include "shaders/shared_structs.h"
#include "vk_check.h"

namespace rtrt
{

// The OBJ model: Vulkan buffers of object data
struct ObjData
{
    uint32_t NbIndices{0};
    uint32_t NbVertices{0};
    glm::mat4 Transform;        // Instance matrix of the object
    BufferWrap VertexBuffer;    // Buffer of vertices
    BufferWrap IndexBuffer;     // Buffer of triangle indices
    BufferWrap MatColorBuffer;  // Buffer of materials
    BufferWrap MatIndexBuffer;  // Buffer of each triangle's material index
};

template <typename Handle>
void SetDebugName(VkDevice Device, Handle Object, VkObjectType Type, const char* Name)
{
    VkDebugUtilsObjectNameInfoEXT Info{
        VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    Info.objectType = Type;
    Info.objectHandle = reinterpret_cast<uint64_t>(Object);
    Info.pObjectName = Name;
    RTRT_CHECK_VK(vkSetDebugUtilsObjectNameEXT(Device, &Info));
}

// Pair each instance with its instance transform
struct ObjInst
{
    glm::mat4 Transform;  // Matrix of the instance
    uint32_t ObjIndex;    // Model index
};

class App;

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
    explicit Renderer(App* Owner);
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Creates the device, loads the scene and builds every pipeline. False if no GPU
    // supports the required extensions.
    bool Init();

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

    // Helpers the acceleration-structure builder uses.
    VkDevice GetDevice() const
    {
        return Device;
    }

    // A one-time command buffer; SubmitTempCmdBuffer runs it, waits and frees it.
    VkCommandBuffer CreateTempCmdBuffer();
    void SubmitTempCmdBuffer(VkCommandBuffer CmdBuffer);

    BufferWrap CreateBufferWrap(VkDeviceSize Size, VkBufferUsageFlags Usage,
                                VkMemoryPropertyFlags Properties);

    // A device-local buffer filled through a staging buffer, using CmdBuf for the copy.
    BufferWrap CreateStagedBufferWrap(VkCommandBuffer CmdBuf, VkDeviceSize Size,
                                      const void* Data, VkBufferUsageFlags Usage);
    template <typename T>
    BufferWrap CreateStagedBufferWrap(VkCommandBuffer CmdBuf, const std::vector<T>& Data,
                                      VkBufferUsageFlags Usage)
    {
        return CreateStagedBufferWrap(CmdBuf, sizeof(T) * Data.size(), Data.data(),
                                      Usage);
    }

private:
    std::vector<const char*> ReqInstanceExtensions = {
        // GLFW will add to this
        "VK_EXT_debug_utils",  // Can help debug validation errors
        "VK_KHR_win32_surface", "VK_KHR_surface"};

    std::vector<const char*> ReqInstanceLayers = {
        "VK_LAYER_KHRONOS_validation"  // ALWAYS!!
    };

    std::vector<const char*> ReqDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,  // Presentation engine; draws to screen
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,  // Ray tracing extension
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,    // Ray tracing extension
        // Required by ray tracing pipeline;
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};

    App* Application;

    RenderSettings Settings;
    // Frames rendered since the history was last reset
    int FrameCount = 0;
    bool ResetHistory = false;

    bool UseSlang = false;
    std::string LoadShader(const char* GlslSpv, const char* SlangSpv);

    double GpuWaitMs = 0.0;
    double RecordMs = 0.0;

    // False if the window closed while minimized
    bool RecreateSizedResources();
    // Everything that depends on the window size.
    void DestroySizedResources();
    void WriteSizedDescriptors();
    VkShaderModule CreateShaderModule(const std::string& Code);
    VkPipelineShaderStageCreateInfo CreateShaderStageInfo(
        const std::string& Code, VkShaderStageFlagBits Stage,
        const char* EntryPoint = "main");

    // Vulkan objects that will be created and the functions that will do the creation.
    VkInstance Instance{};
    void CreateInstance(bool DoApiDump);

    VkPhysicalDevice PhysicalDevice{};
    bool CreatePhysicalDevice();
    bool SupportsRequiredExtensions(VkPhysicalDevice Candidate);

    uint32_t GraphicsQueueIndex{VK_QUEUE_FAMILY_IGNORED};
    void ChooseQueueIndex();

    VkDevice Device{};
    void CreateDevice();

    VkQueue Queue{};
    void GetCommandQueue();

    void LoadExtensions();

    VkSurfaceKHR Surface{};
    void GetSurface();

    VkCommandPool CmdPool{VK_NULL_HANDLE};
    VkCommandBuffer CommandBuffer{};
    void CreateCommandPool();

    VkSwapchainKHR Swapchain{VK_NULL_HANDLE};
    uint32_t ImageCount{0};
    std::vector<VkImage> SwapchainImages{};  // from vkGetSwapchainImagesKHR
    std::vector<VkImageView> ImageViews{};
    // Filled in  VkImageMemoryBarrier objects
    std::vector<VkImageMemoryBarrier> Barriers{};
    VkFence WaitFence{};
    VkSemaphore ReadSemaphore{};
    std::vector<VkSemaphore> WrittenSemaphores{};  // One per swapchain image
    VkExtent2D WindowSize{0, 0};                   // Size of the window
    void CreateSwapchain();
    VkSurfaceFormatKHR ChooseSurfaceFormat();
    uint32_t ChooseSwapchainImageCount(const VkSurfaceCapabilitiesKHR& Capabilities);
    VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& Capabilities);
    void CreateSwapchainImageViews(VkFormat SurfaceFormat);
    // Moves every swapchain image to the present layout.
    void TransitionSwapchainImages();
    void CreateFrameSyncObjects();
    void DestroySwapchain();

    ImageWrap DepthImage;
    void CreateDepthResource();

    VkPipelineLayout PostPipelineLayout{VK_NULL_HANDLE};
    VkRenderPass PostRenderPass{};
    void CreatePostRenderPass();

    std::vector<VkFramebuffer> Framebuffers{};  // One frambuffer per swapchain image.
    void CreatePostFrameBuffers();

    VkPipeline PostPipeline{VK_NULL_HANDLE};
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
    VkPipeline CreateGraphicsPipeline(
        const GraphicsShaders& Shaders,
        const VkPipelineVertexInputStateCreateInfo& VertexInput, VkPipelineLayout Layout,
        VkRenderPass RenderPass);
    FixedFunctionState MakeFixedFunctionState() const;

    VkDescriptorPool ImguiDescPool{VK_NULL_HANDLE};
    void InitGui();

    VkRenderPass ScanlineRenderPass{VK_NULL_HANDLE};
    VkFramebuffer ScanlineFramebuffer{VK_NULL_HANDLE};
    void CreateScanlineRenderPass();
    void CreateScanlineFramebuffer();

    ImageWrap ScImageBuffer{};
    void CreateScBuffer();

    // Each destroys one part of the renderer during shutdown.
    void DestroyPostResources();
    void DestroySceneResources();
    void DestroyScanlineResources();
    void DestroyRaytracingResources();
    void DestroyDenoiseResources();

    ImageWrap RtColCurrBuffer{};
    ImageWrap RtColPrevBuffer{};

    ImageWrap RtKdCurrBuffer{};
    ImageWrap RtKdPrevBuffer{};

    ImageWrap RtNdCurrBuffer{};
    ImageWrap RtNdPrevBuffer{};

    void CreateRtBuffers();

    ImageWrap DenoiseBuffer{};
    void CreateDenoiseBuffer();

    // Arrays of objects instances and textures in the scene
    std::vector<ObjData> ObjDataList{};    // Obj data in Vulkan Buffers
    std::vector<ObjDesc> ObjDescList{};    // Device-addresses of those buffers
    std::vector<ImageWrap> ObjTextures{};  // All textures of the scene
    std::vector<ObjInst> ObjInstList{};    // Instances paring an object and a transform
    BufferWrap LightBuff{};                // Buffer of light list
    void MyLoadModel(const std::string& Filename, const glm::mat4& Transform);

    BufferWrap ObjDescriptionBw{};  // Device buffer of the OBJ descriptions
    void CreateObjDescriptionBuffer();

    DescriptorWrap ScDesc{};
    void CreateScDescriptorSet();

    VkPipelineLayout ScanlinePipelineLayout{};
    VkPipeline ScanlinePipeline{};
    void CreateScPipeline();

    BufferWrap MatrixBw{};  // Device-Host of the camera matrices
    void CreateMatrixBuffer();

    RaytracingBuilder RtBuilder{};
    float MaxAnis = 0;
    std::vector<Emitter> LightList;
    PushConstantRay PcRay{};  // Push constant for ray tracer
    PushConstantDenoise PcDenoise{};
    uint32_t HandleSize{};
    uint32_t HandleAlignment{};
    uint32_t BaseAlignment{};
    void InitRayTracing();

    BlasInput ObjectToVkGeometryKhr(const ObjData& Model);
    void CreateRtAccelerationStructure();

    DescriptorWrap RtDesc{};
    void CreateRtDescriptorSet();

    VkPipelineLayout RtPipelineLayout{};
    VkPipeline RtPipeline{};
    void CreateRtPipeline();
    // One raygen, one miss and one closest-hit shader, each in its own group.
    void CollectRtShaders(std::vector<VkPipelineShaderStageCreateInfo>& Stages,
                          std::vector<VkRayTracingShaderGroupCreateInfoKHR>& Groups);
    void CreateRtPipelineLayout();

    BufferWrap ShaderBindingTableBw;
    VkStridedDeviceAddressRegionKHR RgenRegion{};
    VkStridedDeviceAddressRegionKHR MissRegion{};
    VkStridedDeviceAddressRegionKHR HitRegion{};
    VkStridedDeviceAddressRegionKHR CallRegion{};
    void CreateRtShaderBindingTable();
    // Fills the stride and size of each region from the device's handle alignment.
    void ComputeSbtRegions(uint32_t MissCount, uint32_t HitCount);
    // Copies the raygen, miss and hit handles into the mapped staging buffer.
    void WriteSbtHandles(const BufferWrap& Staging, VkDeviceSize SbtSize,
                         const std::vector<uint8_t>& Handles, uint32_t MissCount,
                         uint32_t HitCount);

    DescriptorWrap PostDesc{};
    void CreatePostDescriptor();

    DescriptorWrap DenoiseDesc{};
    void CreateDenoiseDescriptorSet();

    VkPipelineLayout DenoiseCompPipelineLayout{};
    VkPipeline DenoisePipeline{};
    void CreateDenoiseCompPipeline();

    void CmdCopyImage(ImageWrap& Src, ImageWrap& Dst);

    void ImageLayoutBarrier(VkCommandBuffer Cmdbuffer, VkImage Image,
                            VkImageLayout OldImageLayout, VkImageLayout NewImageLayout,
                            VkImageAspectFlags AspectMask = VK_IMAGE_ASPECT_COLOR_BIT);
    // Run loop
    bool PrepareFrame();

    glm::mat4 PriorViewProj{};
    void UpdateCameraBuffer();
    void Rasterize();
    void Raytrace();
    void Denoise();

    uint32_t SwapchainIndex{0};

    void PostProcess();
    void SubmitFrame();

    std::string LoadFile(const std::string& Filename);
    uint32_t FindMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags Properties);

    void CopyBuffer(VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize Size);

    void TransitionImageLayout(VkImage Image, VkFormat Format, VkImageLayout OldLayout,
                               VkImageLayout NewLayout, uint32_t MipLevels = 1);
    void CopyBufferToImage(VkBuffer Buffer, VkImage Image, uint32_t Width,
                           uint32_t Height);

    ImageWrap CreateTextureImage(const std::string& FileName);
    ImageWrap CreateBufferImage(VkExtent2D& Size);

    ImageWrap CreateImageWrap(uint32_t Width, uint32_t Height, VkFormat Format,
                              VkImageUsageFlags Usage, VkMemoryPropertyFlags Properties,
                              uint32_t MipLevels = 1);

    VkImageView CreateImageView(VkImage Image, VkFormat Format,
                                VkImageAspectFlagBits Aspect = VK_IMAGE_ASPECT_COLOR_BIT);
    VkSampler CreateTextureSampler();

    void GenerateMipmaps(VkImage Image, VkFormat ImageFormat, int32_t TexWidth,
                         int32_t TexHeight, uint32_t MipLevels);
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_H_
