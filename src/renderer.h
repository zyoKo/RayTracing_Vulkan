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

class Renderer
{
public:
    std::vector<const char*> ReqInstanceExtensions = {  // GLFW will add to this
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
    explicit Renderer(App* Owner);
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool Init();  // False if no GPU supports the required extensions

    // Variables managed by ImGui
    int FrameCount = 0;  // Frames rendered since the history was last reset
    bool ResetHistory = false;
    bool DenoiseEnabled = true;
    int ViewMode = kViewFinal;
    float Exposure = 1.0f;
    float Gamma = 2.2f;
    void SetDefaultSettings();

    bool UseSlang = false;
    std::string LoadShader(const char* GlslSpv, const char* SlangSpv);
    void SetShaderLanguage(bool UseSlangShaders);  // Rebuilds every pipeline

    double GpuWaitMs = 0.0;  // Blocked on the previous frame's fence
    double RecordMs = 0.0;   // Acquire, record, submit and present
    void DrawFrame();

    void DestroyAllVulkanResources();

    // Some auxiliary functions
    bool RecreateSizedResources();  // False if the window closed while minimized
    VkCommandBuffer CreateTempCmdBuffer();
    void SubmitTempCmdBuffer(VkCommandBuffer CmdBuffer);
    VkShaderModule CreateShaderModule(const std::string& Code);
    VkPipelineShaderStageCreateInfo CreateShaderStageInfo(
        const std::string& Code, VkShaderStageFlagBits Stage,
        const char* EntryPoint = "main");

    // Vulkan objects that will be created and the functions that will do the creation.
    VkInstance Instance{};
    void CreateInstance(bool DoApiDump);

    VkPhysicalDevice PhysicalDevice{};
    bool CreatePhysicalDevice();

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

    VkDescriptorPool ImguiDescPool{VK_NULL_HANDLE};
    void InitGui();

    VkRenderPass ScanlineRenderPass{VK_NULL_HANDLE};
    VkFramebuffer ScanlineFramebuffer{VK_NULL_HANDLE};
    void CreateScanlineRenderPass();
    void CreateScanlineFramebuffer();

    ImageWrap ScImageBuffer{};
    void CreateScBuffer();

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
    int NumAtrousIterations = 5;
    PushConstantDenoise PcDenoise{};
    uint32_t HandleSize{};
    uint32_t HandleAlignment{};
    uint32_t BaseAlignment{};
    void InitRayTracing();

    // // Accelleration structure objects and functions

    BufferWrap Scratch1;
    BufferWrap Scratch2;
    BlasInput ObjectToVkGeometryKhr(const ObjData& Model);
    void CreateRtAccelerationStructure();

    DescriptorWrap RtDesc{};
    void CreateRtDescriptorSet();

    VkPipelineLayout RtPipelineLayout{};
    VkPipeline RtPipeline{};
    void CreateRtPipeline();

    BufferWrap ShaderBindingTableBw;
    VkStridedDeviceAddressRegionKHR RgenRegion{};
    VkStridedDeviceAddressRegionKHR MissRegion{};
    VkStridedDeviceAddressRegionKHR HitRegion{};
    VkStridedDeviceAddressRegionKHR CallRegion{};
    void CreateRtShaderBindingTable();

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
    bool UseRaytracer = true;
    bool PrepareFrame();
    void ResetRtAccumulation();

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

    BufferWrap CreateStagedBufferWrap(VkCommandBuffer CmdBuf, VkDeviceSize Size,
                                      const void* Data, VkBufferUsageFlags Usage);
    template <typename T>
    BufferWrap CreateStagedBufferWrap(VkCommandBuffer CmdBuf, const std::vector<T>& Data,
                                      VkBufferUsageFlags Usage)
    {
        return CreateStagedBufferWrap(CmdBuf, sizeof(T) * Data.size(), Data.data(),
                                      Usage);
    }

    BufferWrap CreateBufferWrap(VkDeviceSize Size, VkBufferUsageFlags Usage,
                                VkMemoryPropertyFlags Properties);

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
