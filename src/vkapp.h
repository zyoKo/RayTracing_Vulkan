
#pragma once

#include <algorithm>
#include "vulkan/vulkan_core.h"
//#include <vulkan/vulkan.hpp>  // A modern C++ API for Vulkan. Beware 14K lines of code

// Imgui
#define GUI

#ifdef GUI
#include "backends/imgui_impl_glfw.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#endif

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include "shaders/shared_structs.h"
#include "buffer_wrap.h"
#include "image_wrap.h"
#include "descriptor_wrap.h"
#include "acceleration_wrap.h"

//#include "raytracing_wrap.h"
#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>

// The OBJ model: Vulkan buffers of object data
struct ObjData
{
    uint32_t     nbIndices{0};
    uint32_t     nbVertices{0};
    glm::mat4 transform;        // Instance matrix of the object
    BufferWrap vertexBuffer;    // Buffer of vertices 
    BufferWrap indexBuffer;     // Buffer of triangle indices
    BufferWrap matColorBuffer;  // Buffer of materials
    BufferWrap matIndexBuffer;  // Buffer of each triangle's material index
};

#define NAME(handle, objType, name)  { \
        const VkDebugUtilsObjectNameInfoEXT imageNameInfo = {\
            VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, \
            NULL, \
            objType, \
            (uint64_t)handle, name\
        }; \
        vkSetDebugUtilsObjectNameEXT(m_device, &imageNameInfo); }


// Pair each instance with its instance transform
struct ObjInst
{
    glm::mat4 transform;    // Matrix of the instance
    uint32_t  objIndex;     // Model index
};

class App;

class VkApp
{
public:
    std::vector<const char*> reqInstanceExtensions = {  // GLFW will add to this
        "VK_EXT_debug_utils",                            // Can help debug validation errors
        "VK_KHR_win32_surface",
        "VK_KHR_surface"
    };
    
    std::vector<const char*> reqInstanceLayers = {
        //"VK_LAYER_LUNARG_api_dump",  // Useful, but prefer to add (or not) at runtime
        "VK_LAYER_KHRONOS_validation"  // ALWAYS!!
    };
    
    std::vector<const char*> reqDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,		 // Presentation engine; draws to screen
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,	 // Ray tracing extension
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,	 // Ray tracing extension
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME}; // Required by ray tracing pipeline;
    
    App* app;
    VkApp(App* _app);

    // @@ Variables managed by ImGui
    int frameCount;
    void drawFrame();

    void destroyAllVulkanResources();

    // Some auxiliary functions
    void recreateSizedResources(VkExtent2D size);
    VkCommandBuffer createTempCmdBuffer();
    void submitTempCmdBuffer(VkCommandBuffer cmdBuffer);
    VkShaderModule createShaderModule(std::string code);
    VkPipelineShaderStageCreateInfo createShaderStageInfo(const std::string&    code,
                                                          VkShaderStageFlagBits stage,
                                                          const char* entryPoint = "main");
                            
    // Vulkan objects that will be created and the functions that will do the creation.
    VkInstance m_instance{};
    void createInstance(bool doApiDump);

    VkPhysicalDevice m_physicalDevice{};
    void createPhysicalDevice();

    uint32_t m_graphicsQueueIndex{VK_QUEUE_FAMILY_IGNORED};
    void chooseQueueIndex();

    VkDevice m_device{};
    void createDevice();

    VkQueue m_queue{};
    void getCommandQueue();
    
    void loadExtensions();
    
    VkSurfaceKHR m_surface{};
    void getSurface();
    
    VkCommandPool m_cmdPool{VK_NULL_HANDLE};
    VkCommandBuffer m_commandBuffer{};
    void createCommandPool();

    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    uint32_t       m_imageCount{0};
    std::vector<VkImage>     m_swapchainImages{};  // from vkGetSwapchainImagesKHR
    std::vector<VkImageView> m_imageViews{};
    std::vector<VkImageMemoryBarrier> m_barriers{};  // Filled in  VkImageMemoryBarrier objects
    VkFence m_waitFence{}; 
    VkSemaphore m_readSemaphore{};
    VkSemaphore m_writtenSemaphore{};
    VkExtent2D windowSize{0, 0}; // Size of the window
    void createSwapchain();
    void destroySwapchain();

    ImageWrap m_depthImage;
    void createDepthResource();
    
    VkPipelineLayout m_postPipelineLayout{VK_NULL_HANDLE};
    VkRenderPass m_postRenderPass{};
    void createPostRenderPass();
    
    std::vector<VkFramebuffer> m_framebuffers{}; // One frambuffer per swapchain image.
    void createPostFrameBuffers();

    VkPipeline m_postPipeline{VK_NULL_HANDLE};
    void createPostPipeline();
    
    #ifdef GUI
    VkDescriptorPool m_imguiDescPool{VK_NULL_HANDLE};
    void initGUI();
    #endif
    
    VkRenderPass m_scanlineRenderPass{VK_NULL_HANDLE};
    VkFramebuffer m_scanlineFramebuffer{VK_NULL_HANDLE};
    void createScanlineRenderPass();

    ImageWrap m_scImageBuffer{};
    void createScBuffer();

    void destroyRaytracingResources();
    void destroyDenoiseResources();
    
    ImageWrap m_rtColCurrBuffer{};
    ImageWrap m_rtColPrevBuffer{};
    
    ImageWrap m_rtKdCurrBuffer{};
    ImageWrap m_rtKdPrevBuffer{};
    
    ImageWrap m_rtNdCurrBuffer{};
    ImageWrap m_rtNdPrevBuffer{};
    
    void createRtBuffers();
    
    ImageWrap m_denoiseBuffer{};
    void createDenoiseBuffer();

    // Arrays of objects instances and textures in the scene
    std::vector<ObjData>  m_objData{};  // Obj data in Vulkan Buffers
    std::vector<ObjDesc>  m_objDesc{};  // Device-addresses of those buffers
    std::vector<ImageWrap>  m_objText{}; // All textures of the scene
    std::vector<ObjInst>  m_objInst{}; // Instances paring an object and a transform
    BufferWrap m_lightBuff{};          // Buffer of light list
    void myloadModel(const std::string& filename, glm::mat4 transform);

    BufferWrap m_objDescriptionBW{};  // Device buffer of the OBJ descriptions
    void createObjDescriptionBuffer();

    DescriptorWrap m_scDesc{};
    void createScDescriptorSet();

    VkPipelineLayout            m_scanlinePipelineLayout{};
    VkPipeline                  m_scanlinePipeline{};
    void createScPipeline();

    BufferWrap m_matrixBW{};  // Device-Host of the camera matrices
    void   createMatrixBuffer();
    
    RaytracingBuilderKHR m_rtBuilder{};
    float m_maxAnis = 0;
    std::vector<Emitter> lightList;
    PushConstantRay m_pcRay{};  // Push constant for ray tracer
    int m_num_atrous_iterations = 5;
    PushConstantDenoise m_pcDenoise{};
    uint32_t handleSize{};
    uint32_t handleAlignment{};
    uint32_t baseAlignment{};
    void initRayTracing();

    // // Accelleration structure objects and functions

    BufferWrap m_scratch1;
    BufferWrap m_scratch2;
    BlasInput objectToVkGeometryKHR(const ObjData& model);
    void createBottomLevelAS();
	void createTopLevelAS();
    void createRtAccelerationStructure();

    DescriptorWrap m_rtDesc{};
    void createRtDescriptorSet();

    VkPipelineLayout m_rtPipelineLayout{};
    VkPipeline       m_rtPipeline{};
    void createRtPipeline();
    
    BufferWrap m_shaderBindingTableBW;
    VkStridedDeviceAddressRegionKHR m_rgenRegion{};
    VkStridedDeviceAddressRegionKHR m_missRegion{};
    VkStridedDeviceAddressRegionKHR m_hitRegion{};
    VkStridedDeviceAddressRegionKHR m_callRegion{};
    void createRtShaderBindingTable();

    DescriptorWrap m_postDesc{};
    void createPostDescriptor();

    DescriptorWrap m_denoiseDesc{};
    void createDenoiseDescriptorSet();
    
    VkPipelineLayout m_denoiseCompPipelineLayout{};
    VkPipeline       m_denoisePipeline{};
    void createDenoiseCompPipeline();

    void CmdCopyImage(ImageWrap& src, ImageWrap& dst);

    void imageLayoutBarrier(VkCommandBuffer cmdbuffer,
                            VkImage image,
                            VkImageLayout oldImageLayout,
                            VkImageLayout newImageLayout,
                            VkImageAspectFlags aspectMask=VK_IMAGE_ASPECT_COLOR_BIT);
    // Run loop 
    bool useRaytracer = true;
    void prepareFrame();
    void ResetRtAccumulation();
    
    glm::mat4 m_priorViewProj{};
    void updateCameraBuffer();
    void rasterize();
    void raytrace();
    void denoise();
    
    uint32_t m_swapchainIndex{0};
    
    void postProcess();
    void submitFrame();
    
    std::string loadFile(const std::string& filename);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    BufferWrap createStagedBufferWrap(const VkCommandBuffer& cmdBuf,
                                      const VkDeviceSize&    size,
                                      const void*            data,
                                      VkBufferUsageFlags     usage);
    template <typename T>
    BufferWrap createStagedBufferWrap(const VkCommandBuffer& cmdBuf,
                                      const std::vector<T>&  data,
                                      VkBufferUsageFlags     usage)
    {
        return createStagedBufferWrap(cmdBuf, sizeof(T)*data.size(), data.data(), usage);
    }
    

    BufferWrap createBufferWrap(VkDeviceSize size, VkBufferUsageFlags usage,
                                VkMemoryPropertyFlags properties);

     void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    
    void transitionImageLayout(VkImage image, VkFormat format,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               uint32_t mipLevels=1);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    
    ImageWrap createTextureImage(std::string fileName);
    ImageWrap createBufferImage(VkExtent2D& size);
    
    ImageWrap createImageWrap(uint32_t width, uint32_t height,
                              VkFormat format,
                              VkImageUsageFlags usage,
                              VkMemoryPropertyFlags properties,
                              uint32_t mipLevels=1);

    VkImageView createImageView(VkImage image, VkFormat format,
                                VkImageAspectFlagBits aspect=VK_IMAGE_ASPECT_COLOR_BIT);
    VkSampler createTextureSampler();
    
    void generateMipmaps(VkImage image, VkFormat imageFormat,
                         int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
};
