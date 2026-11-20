#include "renderer.h"

#include <assert.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include "app.h"
#include "extensions_vk.hpp"
#include "vk_check.h"

namespace rtrt
{

void Renderer::DestroyAllVulkanResources()
{
    RTRT_CHECK_VK(vkDeviceWaitIdle(Device));

    // All objects created on Device must be destroyed before Device.
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(Device, ImguiDescPool, nullptr);

    DestroySwapchain();
    vkDestroySurfaceKHR(Instance, Surface, nullptr);
    vkDestroyCommandPool(Device, CmdPool, nullptr);

    DestroyPostResources();
    DestroySceneResources();
    DestroyScanlineResources();
    DestroyRaytracingResources();
    DestroyDenoiseResources();

    vkDestroyDevice(Device, nullptr);
    vkDestroyInstance(Instance, nullptr);
}

void Renderer::DestroyPostResources()
{
    DepthImage.Destroy(Device);
    vkDestroyRenderPass(Device, PostRenderPass, nullptr);

    for (auto& Framebuffer : Framebuffers)
    {
        vkDestroyFramebuffer(Device, Framebuffer, nullptr);
    }

    ScImageBuffer.Destroy(Device);
    PostDesc.Destroy(Device);

    vkDestroyPipelineLayout(Device, PostPipelineLayout, nullptr);
    vkDestroyPipeline(Device, PostPipeline, nullptr);
}

void Renderer::DestroySceneResources()
{
    for (auto T : ObjTextures)
    {
        T.Destroy(Device);
    }
    // ObjDescList only holds device addresses; the buffers themselves live in
    // ObjDataList.
    for (auto& Ob : ObjDataList)
    {
        Ob.VertexBuffer.Destroy(Device);
        Ob.IndexBuffer.Destroy(Device);
        Ob.MatColorBuffer.Destroy(Device);
        Ob.MatIndexBuffer.Destroy(Device);
    }

    MatrixBw.Destroy(Device);
    ObjDescriptionBw.Destroy(Device);
}

void Renderer::DestroyScanlineResources()
{
    vkDestroyRenderPass(Device, ScanlineRenderPass, nullptr);
    vkDestroyFramebuffer(Device, ScanlineFramebuffer, nullptr);
    ScDesc.Destroy(Device);
    vkDestroyPipelineLayout(Device, ScanlinePipelineLayout, nullptr);
    vkDestroyPipeline(Device, ScanlinePipeline, nullptr);
}

void Renderer::DestroyRaytracingResources()
{
    RtColCurrBuffer.Destroy(Device);
    RtBuilder.Destroy();
    RtDesc.Destroy(Device);
    vkDestroyPipelineLayout(Device, RtPipelineLayout, nullptr);
    vkDestroyPipeline(Device, RtPipeline, nullptr);
    ShaderBindingTableBw.Destroy(Device);

    // History buffers
    RtColPrevBuffer.Destroy(Device);
    RtNdCurrBuffer.Destroy(Device);
    RtNdPrevBuffer.Destroy(Device);
    RtKdCurrBuffer.Destroy(Device);
    RtKdPrevBuffer.Destroy(Device);
}

void Renderer::DestroyDenoiseResources()
{
    DenoiseBuffer.Destroy(Device);
    DenoiseDesc.Destroy(Device);
    vkDestroyPipelineLayout(Device, DenoiseCompPipelineLayout, nullptr);
    vkDestroyPipeline(Device, DenoisePipeline, nullptr);
}

bool Renderer::RecreateSizedResources()
{
    if (!WaitWhileMinimized())
    {
        return false;
    }

    RTRT_CHECK_VK(vkDeviceWaitIdle(Device));
    Application->SetFramebufferResized(false);

    DestroySizedResources();

    CreateSwapchain();  // -> windowSize
    CreateDepthResource();
    CreatePostFrameBuffers();
    CreateScBuffer();
    CreateScanlineFramebuffer();
    CreateRtBuffers();
    CreateDenoiseBuffer();

    WriteSizedDescriptors();

    // Both graphics pipelines bake in the viewport size
    CreatePostPipeline();
    CreateScPipeline();

    ResetRtAccumulation();
    return true;
}

bool Renderer::WaitWhileMinimized()
{
    // A minimized window has a zero-sized framebuffer, which can't back a swapchain.
    int Width = 0, Height = 0;
    glfwGetFramebufferSize(Application->GetWindow(), &Width, &Height);
    while (Width == 0 || Height == 0)
    {
        if (glfwWindowShouldClose(Application->GetWindow()))
        {
            return false;
        }
        glfwWaitEvents();
        glfwGetFramebufferSize(Application->GetWindow(), &Width, &Height);
    }
    return true;
}

void Renderer::DestroySizedResources()
{
    for (auto& Framebuffer : Framebuffers)
    {
        vkDestroyFramebuffer(Device, Framebuffer, nullptr);
    }
    Framebuffers.clear();
    vkDestroyFramebuffer(Device, ScanlineFramebuffer, nullptr);
    DepthImage.Destroy(Device);
    DestroySwapchain();

    ScImageBuffer.Destroy(Device);
    RtColCurrBuffer.Destroy(Device);
    RtColPrevBuffer.Destroy(Device);
    RtNdCurrBuffer.Destroy(Device);
    RtNdPrevBuffer.Destroy(Device);
    RtKdCurrBuffer.Destroy(Device);
    RtKdPrevBuffer.Destroy(Device);
    DenoiseBuffer.Destroy(Device);

    vkDestroyPipeline(Device, PostPipeline, nullptr);
    vkDestroyPipelineLayout(Device, PostPipelineLayout, nullptr);
    vkDestroyPipeline(Device, ScanlinePipeline, nullptr);
    vkDestroyPipelineLayout(Device, ScanlinePipelineLayout, nullptr);
}

void Renderer::WriteSizedDescriptors()
{
    PostDesc.Write(Device, 0, ScImageBuffer.Descriptor());

    RtDesc.Write(Device, 1, RtColCurrBuffer.Descriptor());
    RtDesc.Write(Device, 2, RtColPrevBuffer.Descriptor());
    RtDesc.Write(Device, 3, RtNdCurrBuffer.Descriptor());
    RtDesc.Write(Device, 4, RtNdPrevBuffer.Descriptor());
    RtDesc.Write(Device, 5, RtKdCurrBuffer.Descriptor());
    RtDesc.Write(Device, 6, RtKdPrevBuffer.Descriptor());

    DenoiseDesc.Write(Device, 0, ScImageBuffer.Descriptor());
    DenoiseDesc.Write(Device, 1, DenoiseBuffer.Descriptor());
    DenoiseDesc.Write(Device, 2, RtKdCurrBuffer.Descriptor());
    DenoiseDesc.Write(Device, 3, RtNdCurrBuffer.Descriptor());
}

void Renderer::CreateInstance(bool DoApiDump)
{
    uint32_t CountGLFWextensions{0};
    const char** ReqGLFWextensions =
        glfwGetRequiredInstanceExtensions(&CountGLFWextensions);

    printf("GLFW required extensions count: %d\n", CountGLFWextensions);
    for (uint32_t I = 0; I < CountGLFWextensions; ++I)
    {
        printf("%s\n", ReqGLFWextensions[I]);
    }

    // The api_dump layer must be first on reqInstanceLayers
    if (DoApiDump)
    {
        ReqInstanceLayers.insert(ReqInstanceLayers.begin(), "VK_LAYER_LUNARG_api_dump");
    }

    uint32_t Count;
    // The two step procedure for getting a variable length list
    RTRT_CHECK_VK(vkEnumerateInstanceLayerProperties(&Count, nullptr));
    std::vector<VkLayerProperties> AvailableLayers(Count);
    RTRT_CHECK_VK(vkEnumerateInstanceLayerProperties(&Count, AvailableLayers.data()));

    printf("InstanceLayer count: %d\n", Count);
    for (auto& Layer : AvailableLayers)
    {
        printf("%s\n", Layer.layerName);
    }

    // Another two step dance
    RTRT_CHECK_VK(vkEnumerateInstanceExtensionProperties(nullptr, &Count, nullptr));
    std::vector<VkExtensionProperties> AvailableExtensions(Count);
    RTRT_CHECK_VK(vkEnumerateInstanceExtensionProperties(nullptr, &Count,
                                                         AvailableExtensions.data()));

    printf("InstanceExtensions count: %d\n", Count);
    for (auto& Extensions : AvailableExtensions)
    {
        printf("%s\n", Extensions.extensionName);
    }
    printf("\n");

    VkApplicationInfo ApplicationInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ApplicationInfo.pApplicationName = "rtrt";
    ApplicationInfo.pEngineName = "no-engine";
    ApplicationInfo.apiVersion = VK_MAKE_VERSION(1, 3, 0);

    VkInstanceCreateInfo InstanceCreateInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    InstanceCreateInfo.pNext = nullptr;
    InstanceCreateInfo.pApplicationInfo = &ApplicationInfo;

    InstanceCreateInfo.enabledExtensionCount =
        static_cast<uint32_t>(ReqInstanceExtensions.size());
    InstanceCreateInfo.ppEnabledExtensionNames = ReqInstanceExtensions.data();

    InstanceCreateInfo.enabledLayerCount =
        static_cast<uint32_t>(ReqInstanceLayers.size());
    InstanceCreateInfo.ppEnabledLayerNames = ReqInstanceLayers.data();

    RTRT_CHECK_VK(vkCreateInstance(&InstanceCreateInfo, nullptr, &Instance));
}

bool Renderer::CreatePhysicalDevice()
{
    uint32_t PhysicalDevicesCount;
    RTRT_CHECK_VK(vkEnumeratePhysicalDevices(Instance, &PhysicalDevicesCount, nullptr));
    std::vector<VkPhysicalDevice> PhysicalDevices(PhysicalDevicesCount);
    RTRT_CHECK_VK(vkEnumeratePhysicalDevices(Instance, &PhysicalDevicesCount,
                                             PhysicalDevices.data()));

    printf("%d devices\n", PhysicalDevicesCount);

    // Pick the first GPU that supports every extension in ReqDeviceExtensions.
    for (auto& Candidate : PhysicalDevices)
    {
        VkPhysicalDeviceProperties GPUproperties;
        vkGetPhysicalDeviceProperties(Candidate, &GPUproperties);

        printf("\nGPU: %s", GPUproperties.deviceName);
        if (GPUproperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            printf(" supports ");
        }
        else
        {
            printf(" does not supports ");
        }
        printf("VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU properties!\n");

        if (SupportsRequiredExtensions(Candidate))
        {
            PhysicalDevice = Candidate;
            return true;
        }
    }
    return false;
}

bool Renderer::SupportsRequiredExtensions(VkPhysicalDevice Candidate)
{
    uint32_t ExtCount;
    RTRT_CHECK_VK(
        vkEnumerateDeviceExtensionProperties(Candidate, nullptr, &ExtCount, nullptr));
    std::vector<VkExtensionProperties> ExtensionProperties(ExtCount);
    RTRT_CHECK_VK(vkEnumerateDeviceExtensionProperties(Candidate, nullptr, &ExtCount,
                                                       ExtensionProperties.data()));

    printf("Required Extension Properties for GPU:\n");
    int Count = 0;
    for (const auto& ReqDeviceExtension : ReqDeviceExtensions)
    {
        for (const auto& Properties : ExtensionProperties)
        {
            const int Result = strcmp(ReqDeviceExtension, Properties.extensionName);
            if (!Result)
            {
                printf("%s matches the Required Device Extensions list!\n",
                       Properties.extensionName);
                ++Count;
                break;
            }
        }
    }

    return Count == ReqDeviceExtensions.size();
}

void Renderer::ChooseQueueIndex()
{
    VkQueueFlags RequiredQueueFlags =
        VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

    uint32_t MpCount;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &MpCount, nullptr);
    std::vector<VkQueueFamilyProperties> QueueProperties(MpCount);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &MpCount,
                                             QueueProperties.data());

    // Assumes queue family 0 supports graphics, compute and transfer.
    GraphicsQueueIndex = 0;
}

void Renderer::CreateDevice()
{
    // pNext chain of feature structures:
    //   features2->features11->features12->features13->accelFeature->rtPipelineFeature->NULL
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR RtPipelineFeature{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, nullptr};

    VkPhysicalDeviceAccelerationStructureFeaturesKHR AccelFeature{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        &RtPipelineFeature};

    VkPhysicalDeviceVulkan13Features Features13{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, &AccelFeature};

    VkPhysicalDeviceVulkan12Features Features12{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &Features13};

    VkPhysicalDeviceVulkan11Features Features11{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, &Features12};

    VkPhysicalDeviceFeatures2 Features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                        &Features11};

    // Fill in all structures on the pNext chain
    vkGetPhysicalDeviceFeatures2(PhysicalDevice, &Features2);

    // Bounds-checked buffer access costs performance and isn't needed here
    Features2.features.robustBufferAccess = VK_FALSE;

    float Priority = 1.0;
    VkDeviceQueueCreateInfo QueueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    QueueInfo.queueFamilyIndex = GraphicsQueueIndex;
    QueueInfo.queueCount = 1;
    QueueInfo.pQueuePriorities = &Priority;

    VkDeviceCreateInfo DeviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    DeviceCreateInfo.pNext = &Features2;  // This is the whole pNext chain

    DeviceCreateInfo.queueCreateInfoCount = 1;
    DeviceCreateInfo.pQueueCreateInfos = &QueueInfo;

    DeviceCreateInfo.enabledExtensionCount =
        static_cast<uint32_t>(ReqDeviceExtensions.size());
    DeviceCreateInfo.ppEnabledExtensionNames = ReqDeviceExtensions.data();

    RTRT_CHECK_VK(vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, nullptr, &Device));
}

void Renderer::GetCommandQueue()
{
    vkGetDeviceQueue(Device, GraphicsQueueIndex, 0, &Queue);
    // The queue is owned by the device; nothing to destroy.
}

// Loads function pointers for all registered extensions via
// extensions_vk.cpp, which NVIDIA generates from the Vulkan spec.
void Renderer::LoadExtensions()
{
    load_VK_EXTENSIONS(Instance, vkGetInstanceProcAddr, Device, vkGetDeviceProcAddr);
}

//  VkSurface is Vulkan's name for the screen.  Since GLFW creates and
//  manages the window, it creates the VkSurface at our request.
void Renderer::GetSurface()
{
    VkBool32 IsSupported;  // Supports drawing(presenting) on a screen

    RTRT_CHECK_VK(
        glfwCreateWindowSurface(Instance, Application->GetWindow(), nullptr, &Surface));

    RTRT_CHECK_VK(vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, GraphicsQueueIndex,
                                                       Surface, &IsSupported));

    if (IsSupported != VK_TRUE)
    {
        printf("\nVulkan does not support presenting on this surface!\n");
    }
}

// Create a command pool, used to allocate command buffers, which in
// turn are use to gather and send commands to the GPU.  The flag
// makes it possible to reuse command buffers.  The queue index
// determines which queue the command buffers can be submitted to.
// Use the command pool to also create a command buffer.
void Renderer::CreateCommandPool()
{
    VkCommandPoolCreateInfo PoolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    PoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    PoolCreateInfo.queueFamilyIndex = GraphicsQueueIndex;
    RTRT_CHECK_VK(vkCreateCommandPool(Device, &PoolCreateInfo, nullptr, &CmdPool));

    // Create a command buffer
    VkCommandBufferAllocateInfo AllocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    AllocateInfo.commandPool = CmdPool;
    AllocateInfo.commandBufferCount = 1;
    AllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    RTRT_CHECK_VK(vkAllocateCommandBuffers(Device, &AllocateInfo, &CommandBuffer));
    // Nothing to destroy -- the pool owns the command buffer.
}

void Renderer::CreateSwapchain()
{
    VkSwapchainKHR OldSwapchain = Swapchain;

    RTRT_CHECK_VK(vkDeviceWaitIdle(Device));  // Probably unnecessary

    // Get the surface's capabilities
    VkSurfaceCapabilitiesKHR Capabilities;
    RTRT_CHECK_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface,
                                                            &Capabilities));

    // FIFO is the one present mode every implementation must support.
    VkPresentModeKHR SwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    const VkSurfaceFormatKHR Format = ChooseSurfaceFormat();
    VkFormat SurfaceFormat = Format.format;
    VkColorSpaceKHR SurfaceColor = Format.colorSpace;

    VkExtent2D SwapchainExtent = ChooseSwapchainExtent(Capabilities);
    // A zero size typically means the window is minimized, which isn't handled yet.
    assert(SwapchainExtent.width && SwapchainExtent.height);

    uint32_t SwapchainImageCount = ChooseSwapchainImageCount(Capabilities);
    assert(SwapchainImageCount == 3);  // Other code assumes three swapchain images

    // Create the swap chain
    VkImageUsageFlags ImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_STORAGE_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkSwapchainCreateInfoKHR SwapchainInfo = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    SwapchainInfo.surface = Surface;
    SwapchainInfo.minImageCount = SwapchainImageCount;
    SwapchainInfo.imageFormat = SurfaceFormat;
    SwapchainInfo.imageColorSpace = SurfaceColor;
    SwapchainInfo.imageExtent = SwapchainExtent;
    SwapchainInfo.imageUsage = ImageUsage;
    SwapchainInfo.preTransform = Capabilities.currentTransform;
    SwapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    SwapchainInfo.imageArrayLayers = 1;
    SwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SwapchainInfo.queueFamilyIndexCount = 1;
    SwapchainInfo.pQueueFamilyIndices = &GraphicsQueueIndex;
    SwapchainInfo.presentMode = SwapchainPresentMode;
    SwapchainInfo.oldSwapchain = OldSwapchain;
    SwapchainInfo.clipped = true;

    RTRT_CHECK_VK(vkCreateSwapchainKHR(Device, &SwapchainInfo, nullptr, &Swapchain));

    RTRT_CHECK_VK(vkGetSwapchainImagesKHR(Device, Swapchain, &ImageCount, nullptr));
    SwapchainImages.resize(ImageCount);
    RTRT_CHECK_VK(
        vkGetSwapchainImagesKHR(Device, Swapchain, &ImageCount, SwapchainImages.data()));

    CreateSwapchainImageViews(SurfaceFormat);
    TransitionSwapchainImages();
    CreateFrameSyncObjects();

    WindowSize = SwapchainExtent;
}

uint32_t Renderer::ChooseSwapchainImageCount(const VkSurfaceCapabilitiesKHR& Capabilities)
{
    // Within the bounds supported. Recommendation: minImageCount+1
    uint32_t SwapchainImageCount = Capabilities.minImageCount + 1;
    if (Capabilities.maxImageCount > 0 &&
        SwapchainImageCount > Capabilities.maxImageCount)
    {
        SwapchainImageCount = Capabilities.maxImageCount;
    }
    return SwapchainImageCount;
}

VkSurfaceFormatKHR Renderer::ChooseSurfaceFormat()
{
    uint32_t SurfaceFormatCount;
    RTRT_CHECK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface,
                                                       &SurfaceFormatCount, nullptr));
    std::vector<VkSurfaceFormatKHR> Formats(SurfaceFormatCount);
    RTRT_CHECK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        PhysicalDevice, Surface, &SurfaceFormatCount, Formats.data()));
    return Formats[0];
}

VkExtent2D Renderer::ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& Capabilities)
{
    if (Capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return Capabilities.currentExtent;
    }

    // Does this case ever happen?
    int Width, Height;
    glfwGetFramebufferSize(Application->GetWindow(), &Width, &Height);

    VkExtent2D SwapchainExtent{static_cast<uint32_t>(Width),
                               static_cast<uint32_t>(Height)};
    SwapchainExtent.width =
        std::clamp(SwapchainExtent.width, Capabilities.minImageExtent.width,
                   Capabilities.maxImageExtent.width);
    SwapchainExtent.height =
        std::clamp(SwapchainExtent.height, Capabilities.minImageExtent.height,
                   Capabilities.maxImageExtent.height);
    return SwapchainExtent;
}

void Renderer::CreateSwapchainImageViews(VkFormat SurfaceFormat)
{
    ImageViews.resize(ImageCount);

    for (uint32_t I = 0; I < ImageCount; I++)
    {
        VkImageViewCreateInfo CreateInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        CreateInfo.image = SwapchainImages[I];
        CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        CreateInfo.format = SurfaceFormat;
        CreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        CreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        CreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        CreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        CreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        CreateInfo.subresourceRange.baseMipLevel = 0;
        CreateInfo.subresourceRange.levelCount = 1;
        CreateInfo.subresourceRange.baseArrayLayer = 0;
        CreateInfo.subresourceRange.layerCount = 1;

        RTRT_CHECK_VK(vkCreateImageView(Device, &CreateInfo, nullptr, &ImageViews[I]));
    }
}

void Renderer::TransitionSwapchainImages()
{
    // One barrier per swapchain image, moving it to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR.
    Barriers.resize(ImageCount);
    for (uint32_t I = 0; I < ImageCount; I++)
    {
        VkImageSubresourceRange Range = {0};
        Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Range.baseMipLevel = 0;
        Range.levelCount = VK_REMAINING_MIP_LEVELS;
        Range.baseArrayLayer = 0;
        Range.layerCount = VK_REMAINING_ARRAY_LAYERS;

        VkImageMemoryBarrier MemBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        MemBarrier.dstAccessMask = 0;
        MemBarrier.srcAccessMask = 0;
        MemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        MemBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        MemBarrier.image = SwapchainImages[I];
        MemBarrier.subresourceRange = Range;
        Barriers[I] = MemBarrier;
    }

    VkCommandBuffer Cmd = CreateTempCmdBuffer();
    vkCmdPipelineBarrier(Cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                         ImageCount, Barriers.data());
    SubmitTempCmdBuffer(Cmd);
}

void Renderer::CreateFrameSyncObjects()
{
    // Not part of the swapchain, but used only to synchronize with it.
    VkFenceCreateInfo FenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    FenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    RTRT_CHECK_VK(vkCreateFence(Device, &FenceCreateInfo, nullptr, &WaitFence));

    VkSemaphoreCreateInfo SemCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    RTRT_CHECK_VK(vkCreateSemaphore(Device, &SemCreateInfo, nullptr, &ReadSemaphore));
    // A present may still be waiting on an image's semaphore, so each
    // swapchain image gets its own.
    WrittenSemaphores.resize(ImageCount);
    for (VkSemaphore& Sem : WrittenSemaphores)
    {
        RTRT_CHECK_VK(vkCreateSemaphore(Device, &SemCreateInfo, nullptr, &Sem));
    }
}

void Renderer::DestroySwapchain()
{
    RTRT_CHECK_VK(vkDeviceWaitIdle(Device));

    for (int I = 0; I < 3; ++I)
    {
        vkDestroyImageView(Device, ImageViews[I], nullptr);
    }

    // Destroy the synchronization items:
    vkDestroyFence(Device, WaitFence, nullptr);
    vkDestroySemaphore(Device, ReadSemaphore, nullptr);
    for (VkSemaphore Sem : WrittenSemaphores)
    {
        vkDestroySemaphore(Device, Sem, nullptr);
    }
    WrittenSemaphores.clear();

    // Destroy the actual swapchain:
    vkDestroySwapchainKHR(Device, Swapchain, nullptr);
    Swapchain = VK_NULL_HANDLE;
    ImageViews.clear();
    Barriers.clear();
}

void Renderer::CreateDepthResource()
{
    uint32_t MipLevels = 1;

    // Note DepthImage is type ImageWrap; a tiny wrapper around
    // several related Vulkan objects.
    DepthImage = CreateImageWrap(WindowSize.width, WindowSize.height,
                                 VK_FORMAT_X8_D24_UNORM_PACK32,
                                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MipLevels);
    DepthImage.ImageView = CreateImageView(
        DepthImage.Image, VK_FORMAT_X8_D24_UNORM_PACK32, VK_IMAGE_ASPECT_DEPTH_BIT);
}

// Gets a list of memory types supported by the GPU, and search
// through that list for one that matches the requested properties
// flag.  The (only?) two types requested here are:
//
// (1) VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: For the bulk of the memory
// used by the GPU to store things internally.
//
// (2) VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:
// for memory visible to the CPU  for CPU to GPU copy operations.
uint32_t Renderer::FindMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags Properties)
{
    VkPhysicalDeviceMemoryProperties MemProperties;
    vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemProperties);

    for (uint32_t I = 0; I < MemProperties.memoryTypeCount; I++)
    {
        if ((TypeFilter & (1 << I)) &&
            (MemProperties.memoryTypes[I].propertyFlags & Properties) == Properties)
        {
            return I;
        }
    }

    RTRT_FATAL("No memory type matches filter 0x%x with properties 0x%x", TypeFilter,
               Properties);
}

// A factory function for an ImageWrap, this creates a VkImage and
// creates and binds an associated VkDeviceMemory object.  The
// VkImageView and VkSampler are left empty to be created elsewhere as
// needed.
ImageWrap Renderer::CreateImageWrap(uint32_t Width, uint32_t Height, VkFormat Format,
                                    VkImageUsageFlags Usage,
                                    VkMemoryPropertyFlags Properties, uint32_t MipLevels)
{
    ImageWrap MyImage;

    VkImageCreateInfo ImageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageInfo.extent.width = Width;
    ImageInfo.extent.height = Height;
    ImageInfo.extent.depth = 1;
    ImageInfo.mipLevels = MipLevels;
    ImageInfo.arrayLayers = 1;
    ImageInfo.format = Format;
    ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageInfo.usage = Usage;
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    RTRT_CHECK_VK(vkCreateImage(Device, &ImageInfo, nullptr, &MyImage.Image));

    VkMemoryRequirements MemRequirements;
    vkGetImageMemoryRequirements(Device, MyImage.Image, &MemRequirements);

    VkMemoryAllocateInfo AllocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    AllocInfo.allocationSize = MemRequirements.size;
    AllocInfo.memoryTypeIndex =
        FindMemoryType(MemRequirements.memoryTypeBits, Properties);

    RTRT_CHECK_VK(vkAllocateMemory(Device, &AllocInfo, nullptr, &MyImage.Memory));

    RTRT_CHECK_VK(vkBindImageMemory(Device, MyImage.Image, MyImage.Memory, 0));

    MyImage.ImageView = VK_NULL_HANDLE;
    MyImage.Sampler = VK_NULL_HANDLE;

    return MyImage;
}

VkImageView Renderer::CreateImageView(VkImage Image, VkFormat Format,
                                      VkImageAspectFlagBits Aspect)
{
    VkImageViewCreateInfo ViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ViewInfo.image = Image;
    ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ViewInfo.format = Format;
    ViewInfo.subresourceRange.aspectMask = Aspect;
    ViewInfo.subresourceRange.baseMipLevel = 0;
    ViewInfo.subresourceRange.levelCount = 1;
    ViewInfo.subresourceRange.baseArrayLayer = 0;
    ViewInfo.subresourceRange.layerCount = 1;

    VkImageView ImageView;
    RTRT_CHECK_VK(vkCreateImageView(Device, &ViewInfo, nullptr, &ImageView));
    return ImageView;
}

void Renderer::CreatePostRenderPass()
{
    std::array<VkAttachmentDescription, 2> Attachments{};
    // Color attachment
    Attachments[0].format = VK_FORMAT_B8G8R8A8_UNORM;
    Attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    Attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;

    // Depth attachment
    Attachments[1].format = VK_FORMAT_X8_D24_UNORM_PACK32;
    Attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    Attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;

    const VkAttachmentReference ColorReference{0,
                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference DepthReference{
        1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    std::array<VkSubpassDependency, 1> SubpassDependencies{};
    // Transition from final to initial (VK_SUBPASS_EXTERNAL refers to all commands
    // executed outside of the actual renderpass)
    SubpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    SubpassDependencies[0].dstSubpass = 0;
    SubpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    SubpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    SubpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    SubpassDependencies[0].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    SubpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkSubpassDescription SubpassDescription{};
    SubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SubpassDescription.colorAttachmentCount = 1;
    SubpassDescription.pColorAttachments = &ColorReference;
    SubpassDescription.pDepthStencilAttachment = &DepthReference;

    VkRenderPassCreateInfo RenderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    RenderPassInfo.attachmentCount = static_cast<uint32_t>(Attachments.size());
    RenderPassInfo.pAttachments = Attachments.data();
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &SubpassDescription;
    RenderPassInfo.dependencyCount = static_cast<uint32_t>(SubpassDependencies.size());
    RenderPassInfo.pDependencies = SubpassDependencies.data();

    RTRT_CHECK_VK(vkCreateRenderPass(Device, &RenderPassInfo, nullptr, &PostRenderPass));
}

// A VkFrameBuffer wraps several images into a render target --
// usually a color buffer and a depth buffer.
void Renderer::CreatePostFrameBuffers()
{
    std::array<VkImageView, 2> Fbattachments{};

    // Create frame buffers for every swap chain image
    VkFramebufferCreateInfo FramebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    FramebufferInfo.renderPass = PostRenderPass;
    FramebufferInfo.width = WindowSize.width;
    FramebufferInfo.height = WindowSize.height;
    FramebufferInfo.layers = 1;
    FramebufferInfo.attachmentCount = 2;
    FramebufferInfo.pAttachments = Fbattachments.data();

    // Each of the three swapchain images gets an associated frame
    // buffer, all sharing one depth buffer.
    Framebuffers.resize(ImageCount);
    for (uint32_t I = 0; I < ImageCount; I++)
    {
        Fbattachments[0] = ImageViews[I];  // A color attachment from the swap chain
        Fbattachments[1] = DepthImage.ImageView;  // A depth attachment
        RTRT_CHECK_VK(
            vkCreateFramebuffer(Device, &FramebufferInfo, nullptr, &Framebuffers[I]));
    }
}

void Renderer::CreatePostPipeline()
{
    // Creating the pipeline layout
    VkPipelineLayoutCreateInfo CreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    CreateInfo.setLayoutCount = 1;
    CreateInfo.pSetLayouts = &PostDesc.GetLayout();

    VkPushConstantRange PushConstant{VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                     sizeof(PushConstantPost)};
    CreateInfo.pushConstantRangeCount = 1;
    CreateInfo.pPushConstantRanges = &PushConstant;

    RTRT_CHECK_VK(
        vkCreatePipelineLayout(Device, &CreateInfo, nullptr, &PostPipelineLayout));

    // No geometry in this pipeline's draw.
    VkPipelineVertexInputStateCreateInfo VertexInputInfo{};
    VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputInfo.vertexBindingDescriptionCount = 0;
    VertexInputInfo.pVertexBindingDescriptions = nullptr;
    VertexInputInfo.vertexAttributeDescriptionCount = 0;
    VertexInputInfo.pVertexAttributeDescriptions = nullptr;

    const GraphicsShaders Shaders{"spv/post.vert.spv", "spv/post.vert.slang.spv",
                                  "spv/post.frag.spv", "spv/post.frag.slang.spv"};
    PostPipeline = CreateGraphicsPipeline(Shaders, VertexInputInfo, PostPipelineLayout,
                                          PostRenderPass);
}

VkPipeline Renderer::CreateGraphicsPipeline(
    const GraphicsShaders& Shaders,
    const VkPipelineVertexInputStateCreateInfo& VertexInput, VkPipelineLayout Layout,
    VkRenderPass RenderPass)
{
    VkShaderModule VertShaderModule =
        CreateShaderModule(LoadShader(Shaders.VertGlsl, Shaders.VertSlang));
    VkShaderModule FragShaderModule =
        CreateShaderModule(LoadShader(Shaders.FragGlsl, Shaders.FragSlang));

    VkPipelineShaderStageCreateInfo VertShaderStageInfo{};
    VertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    VertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    VertShaderStageInfo.module = VertShaderModule;
    VertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo FragShaderStageInfo{};
    FragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    FragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    FragShaderStageInfo.module = FragShaderModule;
    FragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo ShaderStages[] = {VertShaderStageInfo,
                                                      FragShaderStageInfo};

    // The state points into itself, so it's wired up here rather than copied around.
    FixedFunctionState State = MakeFixedFunctionState();
    State.ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    State.ViewportState.viewportCount = 1;
    State.ViewportState.pViewports = &State.Viewport;
    State.ViewportState.scissorCount = 1;
    State.ViewportState.pScissors = &State.Scissor;
    State.ColorBlending.pAttachments = &State.ColorBlendAttachment;

    VkGraphicsPipelineCreateInfo PipelineInfo{};
    PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineInfo.stageCount = 2;
    PipelineInfo.pStages = ShaderStages;
    PipelineInfo.pVertexInputState = &VertexInput;
    PipelineInfo.pInputAssemblyState = &State.InputAssembly;
    PipelineInfo.pViewportState = &State.ViewportState;
    PipelineInfo.pRasterizationState = &State.Rasterizer;
    PipelineInfo.pMultisampleState = &State.Multisampling;
    PipelineInfo.pDepthStencilState = &State.DepthStencil;
    PipelineInfo.pColorBlendState = &State.ColorBlending;
    PipelineInfo.layout = Layout;
    PipelineInfo.renderPass = RenderPass;
    PipelineInfo.subpass = 0;
    PipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline Pipeline = VK_NULL_HANDLE;
    RTRT_CHECK_VK(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo,
                                            nullptr, &Pipeline));

    // The pipeline has fully compiled copies of the shaders, so these
    // intermediate (SPV) versions can be destroyed.
    vkDestroyShaderModule(Device, FragShaderModule, nullptr);
    vkDestroyShaderModule(Device, VertShaderModule, nullptr);
    return Pipeline;
}

Renderer::FixedFunctionState Renderer::MakeFixedFunctionState() const
{
    FixedFunctionState State;

    State.InputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    State.InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    State.InputAssembly.primitiveRestartEnable = VK_FALSE;

    State.Viewport.x = 0.0f;
    State.Viewport.y = 0.0f;
    State.Viewport.width = static_cast<float>(WindowSize.width);
    State.Viewport.height = static_cast<float>(WindowSize.height);
    State.Viewport.minDepth = 0.0f;
    State.Viewport.maxDepth = 1.0f;

    State.Scissor.offset = {0, 0};
    State.Scissor.extent = VkExtent2D{WindowSize.width, WindowSize.height};

    State.Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    State.Rasterizer.depthClampEnable = VK_FALSE;
    State.Rasterizer.rasterizerDiscardEnable = VK_FALSE;
    State.Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    State.Rasterizer.lineWidth = 1.0f;
    State.Rasterizer.cullMode = VK_CULL_MODE_NONE;
    State.Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    State.Rasterizer.depthBiasEnable = VK_FALSE;

    State.Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    State.Multisampling.sampleShadingEnable = VK_FALSE;
    State.Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    State.DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    State.DepthStencil.depthTestEnable = VK_TRUE;
    State.DepthStencil.depthWriteEnable = VK_TRUE;
    // BEWARE!!  NECESSARY!!
    State.DepthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    State.DepthStencil.depthBoundsTestEnable = VK_FALSE;
    State.DepthStencil.stencilTestEnable = VK_FALSE;

    State.ColorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    State.ColorBlendAttachment.blendEnable = VK_FALSE;

    State.ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    State.ColorBlending.logicOpEnable = VK_FALSE;
    State.ColorBlending.logicOp = VK_LOGIC_OP_COPY;
    State.ColorBlending.attachmentCount = 1;
    State.ColorBlending.blendConstants[0] = 0.0f;
    State.ColorBlending.blendConstants[1] = 0.0f;
    State.ColorBlending.blendConstants[2] = 0.0f;
    State.ColorBlending.blendConstants[3] = 0.0f;
    return State;
}

std::string Renderer::LoadShader(const char* GlslSpv, const char* SlangSpv)
{
    if (UseSlang)
    {
        std::string Code = LoadFile(SlangSpv);
        if (!Code.empty())
        {
            return Code;
        }
        printf("Slang shader %s not found; using %s\n", SlangSpv, GlslSpv);
    }
    return LoadFile(GlslSpv);
}

void Renderer::SetShaderLanguage(bool UseSlangShaders)
{
    if (UseSlangShaders == UseSlang)
    {
        return;
    }
    UseSlang = UseSlangShaders;

    // The last frame may still be using the old pipelines
    RTRT_CHECK_VK(vkDeviceWaitIdle(Device));

    vkDestroyPipeline(Device, PostPipeline, nullptr);
    vkDestroyPipelineLayout(Device, PostPipelineLayout, nullptr);
    CreatePostPipeline();

    vkDestroyPipeline(Device, ScanlinePipeline, nullptr);
    vkDestroyPipelineLayout(Device, ScanlinePipelineLayout, nullptr);
    CreateScPipeline();

    // The SBT holds group handles of the old pipeline, so it is rebuilt with it
    ShaderBindingTableBw.Destroy(Device);
    vkDestroyPipeline(Device, RtPipeline, nullptr);
    vkDestroyPipelineLayout(Device, RtPipelineLayout, nullptr);
    CreateRtPipeline();
    CreateRtShaderBindingTable();

    vkDestroyPipeline(Device, DenoisePipeline, nullptr);
    vkDestroyPipelineLayout(Device, DenoiseCompPipelineLayout, nullptr);
    CreateDenoiseCompPipeline();
}

std::string Renderer::LoadFile(const std::string& Filename)
{
    std::string Result;
    // ate: Open at file end
    std::ifstream Stream(Filename, std::ios::ate | std::ios::binary);

    if (!Stream.is_open())
    {
        return Result;
    }

    // tellg() is last char position in file (i.e.,  length)
    Result.reserve(Stream.tellg());
    Stream.seekg(0, std::ios::beg);

    Result.assign((std::istreambuf_iterator<char>(Stream)),
                  std::istreambuf_iterator<char>());
    return Result;
}

//----------------------------------------------------------------------------------------
// Post processing pass: tone mapper, UI
void Renderer::PostProcess()
{
    std::array<VkClearValue, 2> ClearValues{};
    ClearValues[0].color = {{1, 1, 1, 1}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo PassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    PassBeginInfo.clearValueCount = 2;
    PassBeginInfo.pClearValues = ClearValues.data();
    PassBeginInfo.renderPass = PostRenderPass;
    PassBeginInfo.framebuffer = Framebuffers[SwapchainIndex];
    PassBeginInfo.renderArea = {{0, 0}, WindowSize};

    vkCmdBeginRenderPass(CommandBuffer, &PassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    {  // extra indent for renderpass commands

        auto AspectRatio =
            static_cast<float>(WindowSize.width) / static_cast<float>(WindowSize.height);
        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PostPipeline);
        vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                PostPipelineLayout, 0, 1, &PostDesc.GetSet(), 0, nullptr);

        // The rasterizer has no G-buffer images, so the debug views only apply to the ray
        // tracer.
        PushConstantPost PcPost{
            Settings.Exposure, Settings.Gamma,
            static_cast<uint32_t>(Settings.UseRaytracer ? Settings.ViewMode
                                                        : kViewFinal)};
        vkCmdPushConstants(CommandBuffer, PostPipelineLayout,
                           VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantPost),
                           &PcPost);

        // Full-screen triangle: the vertex shader generates positions from gl_VertexIndex
        vkCmdDraw(CommandBuffer, 3, 1, 0, 0);

        ImGui::Render();  // Rendering UI
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), CommandBuffer);
    }
    vkCmdEndRenderPass(CommandBuffer);
}

}  // namespace rtrt
