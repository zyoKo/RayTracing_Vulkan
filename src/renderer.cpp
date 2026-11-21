#include "renderer.h"

#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#include <imgui.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "app.h"
#include "vk_check.h"

namespace rtrt
{

Renderer::Renderer(App* Owner) : Application(Owner)
{
}

bool Renderer::Init()
{
    UseSlang = Application->GetUseSlang();  // -slang picks the starting shader language
    if (!GpuContext.Init(Application->GetWindow(), Application->GetDoApiDump()))
    {
        return false;
    }
    if (!CreatePhysicalDevice())
    {
        fprintf(stderr, "No GPU supports the required ray tracing extensions.\n");
        return false;
    }
    ChooseQueueIndex();  // -> GraphicsQueueIndex
    CreateDevice();      // -> Device
    GetCommandQueue();   // -> Queue

    LoadExtensions();  // Auto generated; loads namespace of all known extensions

    CheckPresentSupport();
    CreateCommandPool();  // -> CmdPool

    CreateSwapchain();         // -> Swapchain
    CreateDepthResource();     // -> DepthImage, ...
    CreatePostRenderPass();    // -> PostRenderPass
    CreatePostFrameBuffers();  // -> Framebuffers

    CreateScBuffer();
    CreatePostDescriptor();
    CreatePostPipeline();  // -> PostPipelineLayout

    InitGui();

    MyLoadModel("models/living_room/living_room.obj", glm::mat4(1.0f));

    CreateMatrixBuffer();
    CreateObjDescriptionBuffer();
    CreateScanlineRenderPass();
    CreateScDescriptorSet();
    CreateScPipeline();

    // //init ray tracing capabilities
    CreateRtBuffers();
    InitRayTracing();
    CreateRtAccelerationStructure();
    CreateRtDescriptorSet();
    CreateRtPipeline();
    CreateRtShaderBindingTable();

    CreateDenoiseBuffer();
    CreateDenoiseDescriptorSet();
    CreateDenoiseCompPipeline();
    return true;
}

void Renderer::DrawFrame()
{
    const double Start = glfwGetTime();
    if (!PrepareFrame())
    {
        ImGui::EndFrame();
        return;
    }

    VkCommandBufferBeginInfo BeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    RTRT_CHECK_VK(vkBeginCommandBuffer(CommandBuffer, &BeginInfo));
    {  // Extra indent for code clarity
        UpdateCameraBuffer();

        // Draw scene
        if (Settings.UseRaytracer)
        {
            Raytrace();
            if (Settings.DenoiseEnabled && Settings.ViewMode == kViewFinal)
            {
                Denoise();
            }
        }
        else
        {
            Rasterize();
        }

        PostProcess();  //  tone mapper and output to swapchain image.

    }  // Done recording;  Execute!

    RTRT_CHECK_VK(vkEndCommandBuffer(CommandBuffer));

    SubmitFrame();  // Submit for display
    RecordMs = (glfwGetTime() - Start) * 1000.0 - GpuWaitMs;
}

VkCommandBuffer Renderer::CreateTempCmdBuffer()
{
    VkCommandBufferAllocateInfo AllocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    AllocateInfo.commandBufferCount = 1;
    AllocateInfo.commandPool = CmdPool;
    AllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer CmdBuffer;
    RTRT_CHECK_VK(vkAllocateCommandBuffers(Device, &AllocateInfo, &CmdBuffer));

    VkCommandBufferBeginInfo BeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    RTRT_CHECK_VK(vkBeginCommandBuffer(CmdBuffer, &BeginInfo));
    return CmdBuffer;
}

void Renderer::SubmitTempCmdBuffer(VkCommandBuffer CmdBuffer)
{
    RTRT_CHECK_VK(vkEndCommandBuffer(CmdBuffer));

    VkSubmitInfo SubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &CmdBuffer;
    RTRT_CHECK_VK(vkQueueSubmit(Queue, 1, &SubmitInfo, {}));
    RTRT_CHECK_VK(vkQueueWaitIdle(Queue));
    vkFreeCommandBuffers(Device, CmdPool, 1, &CmdBuffer);
}

bool Renderer::PrepareFrame()
{
    // Use a fence to wait until the command buffer has finished execution before using it
    // again. This must happen before the acquire, so the previous submit's wait on
    // ReadSemaphore is done.
    const double WaitStart = glfwGetTime();
    VkResult WaitResult = VK_TIMEOUT;
    while (WaitResult == VK_TIMEOUT)
    {
        WaitResult = vkWaitForFences(Device, 1, &WaitFence, VK_TRUE, 1'000'000);
    }
    CheckVkResult(WaitResult, "vkWaitForFences", __FILE__, __LINE__);
    GpuWaitMs = (glfwGetTime() - WaitStart) * 1000.0;

    if (Application->GetWindow().GetFramebufferResized() && !RecreateSizedResources())
    {
        return false;
    }

    // Acquire the next image from the swap chain --> SwapchainIndex
    // A suboptimal acquire still signals ReadSemaphore, so that frame is drawn and
    // the swapchain is rebuilt after its present.
    VkResult Result = vkAcquireNextImageKHR(Device, Swapchain, UINT64_MAX, ReadSemaphore,
                                            VK_NULL_HANDLE, &SwapchainIndex);
    while (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if (!RecreateSizedResources())
        {
            return false;
        }
        Result = vkAcquireNextImageKHR(Device, Swapchain, UINT64_MAX, ReadSemaphore,
                                       VK_NULL_HANDLE, &SwapchainIndex);
    }
    CheckVkResult(Result, "vkAcquireNextImageKHR", __FILE__, __LINE__);
    return true;
}

void Renderer::SubmitFrame()
{
    RTRT_CHECK_VK(vkResetFences(Device, 1, &WaitFence));

    // Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
    const VkPipelineStageFlags WaitStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    // The submit info structure specifies a command buffer queue submission batch
    VkSubmitInfo FrameSubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    FrameSubmitInfo.pNext = nullptr;
    FrameSubmitInfo.pWaitDstStageMask = &WaitStageMask;  //  pipeline stages to wait for
    FrameSubmitInfo.waitSemaphoreCount = 1;
    FrameSubmitInfo.pWaitSemaphores = &ReadSemaphore;  // waited upon before execution
    FrameSubmitInfo.signalSemaphoreCount = 1;
    // signaled when execution finishes
    FrameSubmitInfo.pSignalSemaphores = &WrittenSemaphores[SwapchainIndex];
    FrameSubmitInfo.commandBufferCount = 1;
    FrameSubmitInfo.pCommandBuffers = &CommandBuffer;
    RTRT_CHECK_VK(vkQueueSubmit(Queue, 1, &FrameSubmitInfo, WaitFence));

    // Present frame
    VkPresentInfoKHR PresentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = &WrittenSemaphores[SwapchainIndex];
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = &Swapchain;
    PresentInfo.pImageIndices = &SwapchainIndex;
    VkResult Result = vkQueuePresentKHR(Queue, &PresentInfo);
    if (Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR)
    {
        Application->GetWindow().SetFramebufferResized(true);
    }
    else
    {
        CheckVkResult(Result, "vkQueuePresentKHR", __FILE__, __LINE__);
    }
}

VkShaderModule Renderer::CreateShaderModule(const std::string& Code)
{
    VkShaderModuleCreateInfo CreateInfo = {};
    CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    CreateInfo.codeSize = Code.size();
    CreateInfo.pCode = reinterpret_cast<const uint32_t*>(Code.data());

    VkShaderModule ShaderModule = VK_NULL_HANDLE;
    RTRT_CHECK_VK(vkCreateShaderModule(Device, &CreateInfo, nullptr, &ShaderModule));

    return ShaderModule;
}

VkPipelineShaderStageCreateInfo Renderer::CreateShaderStageInfo(
    const std::string& Code, VkShaderStageFlagBits Stage, const char* EntryPoint)
{
    VkPipelineShaderStageCreateInfo ShaderStage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    ShaderStage.stage = Stage;
    ShaderStage.module = CreateShaderModule(Code);
    ShaderStage.pName = EntryPoint;
    return ShaderStage;
}

void Renderer::InitGui()
{
    uint32_t SubpassID = 0;

    // UI
    ImGui::CreateContext();
    ImGuiIO& Io = ImGui::GetIO();
    Io.LogFilename = nullptr;
    Io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    std::vector<VkDescriptorPoolSize> PoolSize{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};
    VkDescriptorPoolCreateInfo PoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    PoolInfo.maxSets = 2;
    PoolInfo.poolSizeCount = 2;
    PoolInfo.pPoolSizes = PoolSize.data();
    RTRT_CHECK_VK(vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &ImguiDescPool));

    // Setup Platform/Renderer back ends
    ImGui_ImplVulkan_InitInfo InitInfo = {};
    InitInfo.Instance = GpuContext.GetInstance();
    InitInfo.PhysicalDevice = PhysicalDevice;
    InitInfo.Device = Device;
    InitInfo.QueueFamily = GraphicsQueueIndex;
    InitInfo.Queue = Queue;
    InitInfo.PipelineCache = VK_NULL_HANDLE;
    InitInfo.DescriptorPool = ImguiDescPool;
    InitInfo.Subpass = SubpassID;
    InitInfo.MinImageCount = 2;
    InitInfo.ImageCount = ImageCount;
    InitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    InitInfo.CheckVkResultFn = nullptr;
    InitInfo.Allocator = nullptr;

    ImGui_ImplVulkan_Init(&InitInfo, PostRenderPass);

    // Upload Fonts
    VkCommandBuffer Cmdbuf = CreateTempCmdBuffer();
    ImGui_ImplVulkan_CreateFontsTexture(Cmdbuf);
    SubmitTempCmdBuffer(Cmdbuf);
    ImGui_ImplVulkan_DestroyFontUploadObjects();  // Staging buffer is no longer needed

    ImGui_ImplGlfw_InitForVulkan(Application->GetWindow().GetHandle(), true);
}

}  // namespace rtrt
