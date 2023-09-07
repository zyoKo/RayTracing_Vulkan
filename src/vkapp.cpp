#include <array>
#include <iostream>     // std::cout
#include <fstream>      // std::ifstream


#ifdef WIN64
#else
#include <unistd.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "vkapp.h"

#include "app.h"

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>

VkApp::VkApp(App* _app) : app(_app)
{
	createInstance(app->doApiDump);	// -> m_instance
	assert(m_instance);
	createPhysicalDevice();		// -> m_physicalDevice i.e. the GPU
	chooseQueueIndex();		    // -> m_graphicsQueueIndex
	createDevice();			    // -> m_device
	getCommandQueue();		    // -> m_queue

	loadExtensions();		    // Auto generated; loads namespace of all known extensions

	getSurface();			    // -> m_surface
	createCommandPool();		// -> m_cmdPool

	createSwapchain();		    // -> m_swapchain
	createDepthResource();		// -> m_depthImage, ...
	createPostRenderPass();		// -> m_postRenderPass
	createPostFrameBuffers();	// -> m_framebuffers

	createScBuffer();
	createPostDescriptor();
	createPostPipeline();		// -> m_postPipelineLayout

	#ifdef GUI
	initGUI();
	#endif

	myloadModel("models/living_room/living_room.obj", glm::mat4(1.0f));

	//createScBuffer();
	//createRtBuffers();

	//createScanlineRenderPass();
	//createStuff();

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
}

void VkApp::drawFrame()
{
	prepareFrame();

	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
	{   // Extra indent for code clarity
		updateCameraBuffer();

		// Draw scene
		if (useRaytracer) 
		{
			raytrace();
			denoise();
		}
		else 
		{
			rasterize();
		}

		postProcess(); //  tone mapper and output to swapchain image.

	}   // Done recording;  Execute!

	vkEndCommandBuffer(m_commandBuffer);

	submitFrame();  // Submit for display
}


VkCommandBuffer VkApp::createTempCmdBuffer()
{
	VkCommandBufferAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocateInfo.commandBufferCount = 1;
	allocateInfo.commandPool = m_cmdPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VkCommandBuffer cmdBuffer;
	vkAllocateCommandBuffers(m_device, &allocateInfo, &cmdBuffer);

	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmdBuffer, &beginInfo);
	return cmdBuffer;
}

void VkApp::submitTempCmdBuffer(VkCommandBuffer cmdBuffer)
{
	vkEndCommandBuffer(cmdBuffer);

	VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;
	vkQueueSubmit(m_queue, 1, &submitInfo, {});
	vkQueueWaitIdle(m_queue);
	vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmdBuffer);
}

void VkApp::prepareFrame()
{
	// Acquire the next image from the swap chain --> m_swapchainIndex
	VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_readSemaphore,
		(VkFence)VK_NULL_HANDLE, &m_swapchainIndex);

	// Check if window has been resized -- or other(??) swapchain specific event
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		recreateSizedResources(windowSize);
	}

	// Use a fence to wait until the command buffer has finished execution before using it again
	while (VK_TIMEOUT == vkWaitForFences(m_device, 1, &m_waitFence, VK_TRUE, 1'000'000))
	{
	}
}

void VkApp::submitFrame()
{
	vkResetFences(m_device, 1, &m_waitFence);

	// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
	const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	// The submit info structure specifies a command buffer queue submission batch
	VkSubmitInfo _si_{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	_si_.pNext = nullptr;
	_si_.pWaitDstStageMask = &waitStageMask; //  pipeline stages to wait for
	_si_.waitSemaphoreCount = 1;
	_si_.pWaitSemaphores = &m_readSemaphore;  // waited upon before execution
	_si_.signalSemaphoreCount = 1;
	_si_.pSignalSemaphores = &m_writtenSemaphore; // signaled when execution finishes
	_si_.commandBufferCount = 1;
	_si_.pCommandBuffers = &m_commandBuffer;
	if (vkQueueSubmit(m_queue, 1, &_si_, m_waitFence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	// Present frame
	VkPresentInfoKHR _i_{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	_i_.waitSemaphoreCount = 1;
	_i_.pWaitSemaphores = &m_writtenSemaphore;;
	_i_.swapchainCount = 1;
	_i_.pSwapchains = &m_swapchain;
	_i_.pImageIndices = &m_swapchainIndex;
	if (vkQueuePresentKHR(m_queue, &_i_) != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}
}


VkShaderModule VkApp::createShaderModule(std::string code)
{
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = (uint32_t*)code.data();

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		assert(0 && "failed to create shader module!");

	return shaderModule;
}

VkPipelineShaderStageCreateInfo VkApp::createShaderStageInfo(const std::string& code,
	VkShaderStageFlagBits stage,
	const char* entryPoint)
{
	VkPipelineShaderStageCreateInfo shaderStage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	shaderStage.stage = stage;
	shaderStage.module = createShaderModule(code);
	shaderStage.pName = entryPoint;
	return shaderStage;
}

#ifdef GUI
void VkApp::initGUI()
{
	uint subpassID = 0;

	// UI
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.LogFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

	std::vector<VkDescriptorPoolSize> poolSize{{VK_DESCRIPTOR_TYPE_SAMPLER, 1}, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }};
	VkDescriptorPoolCreateInfo        poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.maxSets = 2;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSize.data();
	vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_imguiDescPool);

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

	ImGui_ImplGlfw_InitForVulkan(app->GLFW_window, true);
}
#endif
