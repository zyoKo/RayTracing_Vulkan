#include "vkapp.h"

#include <assert.h>

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

VkApp::VkApp(App* _app) : app(_app)
{
}

bool VkApp::Init()
{
    m_useSlang = app->useSlang;      // -slang picks the starting shader language
    createInstance(app->doApiDump);  // -> m_instance
    assert(m_instance);
    if (!createPhysicalDevice())
    {
        fprintf(stderr, "No GPU supports the required ray tracing extensions.\n");
        return false;
    }
    chooseQueueIndex();  // -> m_graphicsQueueIndex
    createDevice();      // -> m_device
    getCommandQueue();   // -> m_queue

    loadExtensions();  // Auto generated; loads namespace of all known extensions

    getSurface();         // -> m_surface
    createCommandPool();  // -> m_cmdPool

    createSwapchain();         // -> m_swapchain
    createDepthResource();     // -> m_depthImage, ...
    createPostRenderPass();    // -> m_postRenderPass
    createPostFrameBuffers();  // -> m_framebuffers

    createScBuffer();
    createPostDescriptor();
    createPostPipeline();  // -> m_postPipelineLayout

    initGUI();

    myloadModel("models/living_room/living_room.obj", glm::mat4(1.0f));

    createMatrixBuffer();
    createObjDescriptionBuffer();
    createScanlineRenderPass();
    createScDescriptorSet();
    createScPipeline();

    // //init ray tracing capabilities
    createRtBuffers();
    initRayTracing();
    createRtAccelerationStructure();
    createRtDescriptorSet();
    createRtPipeline();
    createRtShaderBindingTable();

    createDenoiseBuffer();
    createDenoiseDescriptorSet();
    createDenoiseCompPipeline();
    return true;
}

void VkApp::drawFrame()
{
    const double start = glfwGetTime();
    if (!prepareFrame())
    {
        ImGui::EndFrame();
        return;
    }

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    RTRT_CHECK_VK(vkBeginCommandBuffer(m_commandBuffer, &beginInfo));
    {  // Extra indent for code clarity
        updateCameraBuffer();

        // Draw scene
        if (useRaytracer)
        {
            raytrace();
            if (m_denoiseEnabled && m_viewMode == eViewFinal)
            {
                denoise();
            }
        }
        else
        {
            rasterize();
        }

        postProcess();  //  tone mapper and output to swapchain image.

    }  // Done recording;  Execute!

    RTRT_CHECK_VK(vkEndCommandBuffer(m_commandBuffer));

    submitFrame();  // Submit for display
    m_recordMs = (glfwGetTime() - start) * 1000.0 - m_gpuWaitMs;
}

VkCommandBuffer VkApp::createTempCmdBuffer()
{
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandBufferCount = 1;
    allocateInfo.commandPool = m_cmdPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer cmdBuffer;
    RTRT_CHECK_VK(vkAllocateCommandBuffers(m_device, &allocateInfo, &cmdBuffer));

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    RTRT_CHECK_VK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
    return cmdBuffer;
}

void VkApp::submitTempCmdBuffer(VkCommandBuffer cmdBuffer)
{
    RTRT_CHECK_VK(vkEndCommandBuffer(cmdBuffer));

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    RTRT_CHECK_VK(vkQueueSubmit(m_queue, 1, &submitInfo, {}));
    RTRT_CHECK_VK(vkQueueWaitIdle(m_queue));
    vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmdBuffer);
}

bool VkApp::prepareFrame()
{
    // Use a fence to wait until the command buffer has finished execution before using it
    // again. This must happen before the acquire, so the previous submit's wait on
    // m_readSemaphore is done.
    const double waitStart = glfwGetTime();
    VkResult waitResult = VK_TIMEOUT;
    while (waitResult == VK_TIMEOUT)
    {
        waitResult = vkWaitForFences(m_device, 1, &m_waitFence, VK_TRUE, 1'000'000);
    }
    CheckVkResult(waitResult, "vkWaitForFences", __FILE__, __LINE__);
    m_gpuWaitMs = (glfwGetTime() - waitStart) * 1000.0;

    if (app->m_framebufferResized && !recreateSizedResources())
    {
        return false;
    }

    // Acquire the next image from the swap chain --> m_swapchainIndex
    // A suboptimal acquire still signals m_readSemaphore, so that frame is drawn and
    // the swapchain is rebuilt after its present.
    VkResult result =
        vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_readSemaphore,
                              VK_NULL_HANDLE, &m_swapchainIndex);
    while (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if (!recreateSizedResources())
        {
            return false;
        }
        result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_readSemaphore,
                                       VK_NULL_HANDLE, &m_swapchainIndex);
    }
    CheckVkResult(result, "vkAcquireNextImageKHR", __FILE__, __LINE__);
    return true;
}

void VkApp::submitFrame()
{
    RTRT_CHECK_VK(vkResetFences(m_device, 1, &m_waitFence));

    // Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
    const VkPipelineStageFlags waitStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    // The submit info structure specifies a command buffer queue submission batch
    VkSubmitInfo _si_{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    _si_.pNext = nullptr;
    _si_.pWaitDstStageMask = &waitStageMask;  //  pipeline stages to wait for
    _si_.waitSemaphoreCount = 1;
    _si_.pWaitSemaphores = &m_readSemaphore;  // waited upon before execution
    _si_.signalSemaphoreCount = 1;
    // signaled when execution finishes
    _si_.pSignalSemaphores = &m_writtenSemaphores[m_swapchainIndex];
    _si_.commandBufferCount = 1;
    _si_.pCommandBuffers = &m_commandBuffer;
    RTRT_CHECK_VK(vkQueueSubmit(m_queue, 1, &_si_, m_waitFence));

    // Present frame
    VkPresentInfoKHR _i_{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    _i_.waitSemaphoreCount = 1;
    _i_.pWaitSemaphores = &m_writtenSemaphores[m_swapchainIndex];
    _i_.swapchainCount = 1;
    _i_.pSwapchains = &m_swapchain;
    _i_.pImageIndices = &m_swapchainIndex;
    VkResult result = vkQueuePresentKHR(m_queue, &_i_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        app->m_framebufferResized = true;
    }
    else
    {
        CheckVkResult(result, "vkQueuePresentKHR", __FILE__, __LINE__);
    }
}

VkShaderModule VkApp::createShaderModule(const std::string& code)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    RTRT_CHECK_VK(vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule));

    return shaderModule;
}

VkPipelineShaderStageCreateInfo VkApp::createShaderStageInfo(const std::string& code,
                                                             VkShaderStageFlagBits stage,
                                                             const char* entryPoint)
{
    VkPipelineShaderStageCreateInfo shaderStage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    shaderStage.stage = stage;
    shaderStage.module = createShaderModule(code);
    shaderStage.pName = entryPoint;
    return shaderStage;
}

void VkApp::initGUI()
{
    uint32_t subpassID = 0;

    // UI
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    std::vector<VkDescriptorPoolSize> poolSize{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 2;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSize.data();
    RTRT_CHECK_VK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_imguiDescPool));

    // Setup Platform/Renderer back ends
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_instance;
    init_info.PhysicalDevice = m_physicalDevice;
    init_info.Device = m_device;
    init_info.QueueFamily = m_graphicsQueueIndex;
    init_info.Queue = m_queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_imguiDescPool;
    init_info.Subpass = subpassID;
    init_info.MinImageCount = 2;
    init_info.ImageCount = m_imageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = nullptr;
    init_info.Allocator = nullptr;

    ImGui_ImplVulkan_Init(&init_info, m_postRenderPass);

    // Upload Fonts
    VkCommandBuffer cmdbuf = createTempCmdBuffer();
    ImGui_ImplVulkan_CreateFontsTexture(cmdbuf);
    submitTempCmdBuffer(cmdbuf);
    ImGui_ImplVulkan_DestroyFontUploadObjects();  // Staging buffer is no longer needed

    ImGui_ImplGlfw_InitForVulkan(app->GLFW_window, true);
}

}  // namespace rtrt
