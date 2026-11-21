#include "renderer.h"

#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#include <imgui.h>

#include "vk_check.h"

namespace rtrt
{

bool Renderer::Init(Window& Target, Context& Instance, Device& Gpu, Swapchain& Chain,
                    const Camera& View, bool UseSlangShaders)
{
    MainWindow = &Target;
    GpuContext = &Instance;
    GpuDevice = &Gpu;
    MainSwapchain = &Chain;
    MainCamera = &View;
    UseSlang = UseSlangShaders;

    CreateCommandPool();

    CreateSwapchain();
    CreateFrameFence();
    CreateDepthResource();   // -> DepthImage, ...
    CreatePostRenderPass();  // -> PostRenderPass.GetHandle()
    CreatePostFrameBuffers();

    CreateScBuffer();
    CreatePostDescriptor();
    CreatePostPipeline();  // -> PostPipelineLayout.GetHandle()

    InitGui();

    MainScene.LoadModel(*GpuDevice, MainCmdPool, "models/living_room/living_room.obj",
                        glm::mat4(1.0f));

    CreateMatrixBuffer();
    MainScene.CreateDescriptionBuffer(*GpuDevice, MainCmdPool);
    CreateScanlineRenderPass();
    CreateScDescriptorSet();
    CreateScPipeline();

    // //init ray tracing capabilities
    CreateRtBuffers();
    InitRayTracing();
    CreateRtAccelerationStructure();
    CreateRtDescriptorSet();
    CreateRtPipeline();
    // One raygen, one miss and one hit group
    RtSbt.Create(*GpuDevice, MainCmdPool, RtPipeline.GetHandle(), 1, 1);

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

    FrameCmd.Begin();
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

    FrameCmd.End();

    SubmitFrame();  // Submit for display
    RecordMs = (glfwGetTime() - Start) * 1000.0 - GpuWaitMs;
}

bool Renderer::PrepareFrame()
{
    // Use a fence to wait until the command buffer has finished execution before using it
    // again. This must happen before the acquire, so the previous submit's wait on
    // acquire semaphore is done.
    const double WaitStart = glfwGetTime();
    FrameFence.Wait();
    GpuWaitMs = (glfwGetTime() - WaitStart) * 1000.0;

    if (MainWindow->GetFramebufferResized() && !RecreateSizedResources())
    {
        return false;
    }

    // A suboptimal acquire still signals the acquire semaphore, so that frame is drawn
    // and the swapchain is rebuilt after its present.
    VkResult Result = MainSwapchain->AcquireNextImage();
    while (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if (!RecreateSizedResources())
        {
            return false;
        }
        Result = MainSwapchain->AcquireNextImage();
    }
    CheckVkResult(Result, "vkAcquireNextImageKHR", __FILE__, __LINE__);
    return true;
}

void Renderer::SubmitFrame()
{
    FrameFence.Reset();

    // Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
    const VkPipelineStageFlags WaitStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    const VkSemaphore Acquired = MainSwapchain->GetAcquireSemaphore();
    const VkSemaphore Rendered = MainSwapchain->GetRenderedSemaphore();

    VkSubmitInfo FrameSubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    FrameSubmitInfo.pWaitDstStageMask = &WaitStageMask;
    FrameSubmitInfo.waitSemaphoreCount = 1;
    FrameSubmitInfo.pWaitSemaphores = &Acquired;
    FrameSubmitInfo.signalSemaphoreCount = 1;
    FrameSubmitInfo.pSignalSemaphores = &Rendered;
    FrameSubmitInfo.commandBufferCount = 1;
    const VkCommandBuffer Cmd = FrameCmd.GetHandle();
    FrameSubmitInfo.pCommandBuffers = &Cmd;
    RTRT_CHECK_VK(vkQueueSubmit(GpuDevice->GetQueue(), 1, &FrameSubmitInfo,
                                FrameFence.GetHandle()));

    VkResult Result = MainSwapchain->Present(GpuDevice->GetQueue());
    if (Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR)
    {
        MainWindow->SetFramebufferResized(true);
    }
    else
    {
        CheckVkResult(Result, "vkQueuePresentKHR", __FILE__, __LINE__);
    }
}

void Renderer::InitGui()
{
    uint32_t SubpassID = 0;

    // UI
    ImGui::CreateContext();
    ImGuiIO& Io = ImGui::GetIO();
    Io.LogFilename = nullptr;
    Io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    ImguiDescPool.Create(
        GpuDevice->GetHandle(),
        {{VK_DESCRIPTOR_TYPE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}},
        2);

    // Setup Platform/Renderer back ends
    ImGui_ImplVulkan_InitInfo InitInfo = {};
    InitInfo.Instance = GpuContext->GetInstance();
    InitInfo.PhysicalDevice = GpuDevice->GetPhysical();
    InitInfo.Device = GpuDevice->GetHandle();
    InitInfo.QueueFamily = GpuDevice->GetQueueFamily();
    InitInfo.Queue = GpuDevice->GetQueue();
    InitInfo.PipelineCache = VK_NULL_HANDLE;
    InitInfo.DescriptorPool = ImguiDescPool.GetHandle();
    InitInfo.Subpass = SubpassID;
    InitInfo.MinImageCount = 2;
    InitInfo.ImageCount = MainSwapchain->GetImageCount();
    InitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    InitInfo.CheckVkResultFn = nullptr;
    InitInfo.Allocator = nullptr;

    ImGui_ImplVulkan_Init(&InitInfo, PostRenderPass.GetHandle());

    // Upload Fonts
    VkCommandBuffer Cmdbuf = MainCmdPool.BeginSingleUse();
    ImGui_ImplVulkan_CreateFontsTexture(Cmdbuf);
    MainCmdPool.EndSingleUse(Cmdbuf);
    ImGui_ImplVulkan_DestroyFontUploadObjects();  // Staging buffer is no longer needed

    ImGui_ImplGlfw_InitForVulkan(MainWindow->GetHandle(), true);
}

}  // namespace rtrt
