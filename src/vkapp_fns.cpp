

#include <array>
#include <iostream>     // std::cout
#include <fstream>      // std::ifstream

#include <unordered_set>
#include <unordered_map>

#include "vkapp.h"
#include "app.h"
#include "extensions_vk.hpp"

#ifdef GUI
#include "backends/imgui_impl_glfw.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#endif


void VkApp::destroyAllVulkanResources()
{
    // @@
    vkDeviceWaitIdle(m_device);  // Uncomment this when you have an m_device created.

    // Destroy all vulkan objects.
    // ...  All objects created on m_device must be destroyed before m_device.
    destroySwapchain();
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
    m_depthImage.destroy(m_device);
    vkDestroyRenderPass(m_device, m_postRenderPass, nullptr);

    for (auto& framebuffer : m_framebuffers)
	    vkDestroyFramebuffer(m_device, framebuffer, nullptr);

    m_scImageBuffer.destroy(m_device);
    m_postDesc.destroy(m_device);

    vkDestroyPipelineLayout(m_device, m_postPipelineLayout, nullptr);
    vkDestroyPipeline(m_device, m_postPipeline, nullptr);

    for (auto t : m_objText) t.destroy(m_device);
    //for (auto ob : m_objDesc) ob.destroy(m_device);

    m_matrixBW.destroy(m_device);
    m_objDescriptionBW.destroy(m_device);
    vkDestroyRenderPass(m_device, m_scanlineRenderPass, nullptr);
    vkDestroyFramebuffer(m_device, m_scanlineFramebuffer, nullptr); 
    m_scDesc.destroy(m_device);
    vkDestroyPipelineLayout(m_device, m_scanlinePipelineLayout, nullptr);
    vkDestroyPipeline(m_device, m_scanlinePipeline, nullptr);

    // Project 3 Cleanup
    m_rtColCurrBuffer.destroy(m_device);
    m_rtBuilder.destroy();
    vkDestroyPipelineLayout(m_device, m_rtPipelineLayout, nullptr);
    vkDestroyPipeline(m_device, m_rtPipeline, nullptr);
    m_shaderBindingTableBW.destroy(m_device);

    // Project 5 Cleanup
    m_rtColPrevBuffer.destroy(m_device);
    m_rtNdCurrBuffer.destroy(m_device);
    m_rtNdPrevBuffer.destroy(m_device);
    m_rtKdCurrBuffer.destroy(m_device);
    m_rtKdPrevBuffer.destroy(m_device);

    // Project 6 Cleanup
    m_denoiseBuffer.destroy(m_device);
    m_denoiseDesc.destroy(m_device);
    vkDestroyPipelineLayout(m_device, m_denoiseCompPipelineLayout, nullptr);
    vkDestroyPipeline(m_device, m_denoisePipeline, nullptr);

    // Before this ----
	vkDestroyDevice(m_device, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void VkApp::recreateSizedResources(VkExtent2D size)
{
    assert(false && "Not ready for onResize events.");
    // Destroy everything related to the window size
    // (RE)Create them all at the new size
}
 
void VkApp::createInstance(bool doApiDump)
{
    uint32_t countGLFWextensions{0};
    const char** reqGLFWextensions = glfwGetRequiredInstanceExtensions(&countGLFWextensions);

    // @@
    // Append each GLFW required extension in reqGLFWextensions to reqInstanceExtensions
    // Print them out while your are at it
    printf("GLFW required extensions count: %d\n", countGLFWextensions);
    for (uint32_t i = 0; i < countGLFWextensions; ++i)
        printf("%s\n", reqGLFWextensions[i]);
    // ...

    // Suggestion: Parse a command line argument to set/unset doApiDump
    // If included, the api_dump layer should be first on reqInstanceLayers
    if (doApiDump)
        reqInstanceLayers.insert(reqInstanceLayers.begin(), "VK_LAYER_LUNARG_api_dump");
  
    uint32_t count;
    // The two step procedure for getting a variable length list
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> availableLayers(count);
    vkEnumerateInstanceLayerProperties(&count, availableLayers.data());

    // @@
    // Print out the availableLayers
    // ...  use availableLayers[i].layerName
    printf("InstanceLayer count: %d\n", count);
    for (auto& layer : availableLayers)
        printf("%s\n", layer.layerName);

    // Another two step dance
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, availableExtensions.data());

    // @@
    // Print out the availableExtensions
    // ...  use availableExtensions[i].extensionName
    printf("InstanceExtensions count: %d\n", count);
    for (auto& extensions : availableExtensions)
        printf("%s\n", extensions.extensionName);
    printf("\n");

    VkApplicationInfo applicationInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    applicationInfo.pApplicationName = "rtrt";
    applicationInfo.pEngineName      = "no-engine";
    applicationInfo.apiVersion       = VK_MAKE_VERSION(1, 3, 0);

    VkInstanceCreateInfo instanceCreateInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceCreateInfo.pNext                   = nullptr;
    instanceCreateInfo.pApplicationInfo        = &applicationInfo;
    
    instanceCreateInfo.enabledExtensionCount   = reqInstanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = reqInstanceExtensions.data();
    
    instanceCreateInfo.enabledLayerCount       = reqInstanceLayers.size();
    instanceCreateInfo.ppEnabledLayerNames     = reqInstanceLayers.data();

    // @@
    // Verify VkResult is VK_SUCCESS
    const VkResult success = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (success != VK_SUCCESS)
        printf("Failed to create Instance!\n\n");
    
    // Document with a cut-and-paste of the three list printouts above.
    // To destroy: vkDestroyInstance(m_instance, nullptr);
}

void VkApp::createPhysicalDevice()
{
    uint physicalDevicesCount;
    vkEnumeratePhysicalDevices(m_instance, &physicalDevicesCount, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices(physicalDevicesCount);
    vkEnumeratePhysicalDevices(m_instance, &physicalDevicesCount, physicalDevices.data());

    std::vector<uint32_t> compatibleDevices;
  
    printf("%d devices\n", physicalDevicesCount);
    //int i = 0;

    // For each GPU:
    for (auto& physicalDevice : physicalDevices) {

        // Get the GPU's properties
        VkPhysicalDeviceProperties GPUproperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &GPUproperties);

        // Get the GPU's extension list;  Another two-step list retrieval procedure:
        uint extCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> extensionProperties(extCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                             &extCount, extensionProperties.data());

        // @@ This code is in a loop iterating a variable physicalDevice
        // through a list of all physicalDevices.  The
        // physicalDevice's properties (GPUproperties) and a list of
        // its extension properties (extensionProperties) are retrieved
        // above, and here we judge if the physicalDevice (i.e.. GPU)
        // is compatible with our requirements. We consider a GPU to be
        // compatible if it satisfies both:
        
        //    GPUproperties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        
        // and
        
        //    All reqDeviceExtensions can be found in the GPUs extensionProperties list
        //      That is: for all i, there exists a j such that:
        //                 reqDeviceExtensions[i] == extensionProperties[j].extensionName

        //  If a GPU is found to be compatible
        //  Return it (physicalDevice), or continue the search and then return the best GPU.
        //    raise an exception of none were found
        //    tell me all about your system if more than one was found.

        printf("\nGPU: %s", GPUproperties.deviceName);
        if (GPUproperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            printf(" supports ");
        else
            printf(" does not supports ");
        printf("VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU properties!\n");

        printf("Required Extension Properties for GPU:\n");
        int count = 0;
        for (const auto& reqDeviceExtension : reqDeviceExtensions)
        {
            for (const auto& properties : extensionProperties)
            {
                const int result = strcmp(reqDeviceExtension, properties.extensionName);
                if (!result)
                {
                    printf("%s matches the Required Device Extensions list!\n", properties.extensionName);
                    ++count;
                    break;
                }
            }
        }

        if (count == reqDeviceExtensions.size())
        {
            m_physicalDevice = physicalDevice;
            return;
        }
    }
  
}

void VkApp::chooseQueueIndex()
{
    VkQueueFlags requiredQueueFlags = VK_QUEUE_GRAPHICS_BIT | 
    								  VK_QUEUE_COMPUTE_BIT  | 
									  VK_QUEUE_TRANSFER_BIT;
    
    uint32_t mpCount;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &mpCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueProperties(mpCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &mpCount, queueProperties.data());

    m_graphicsQueueIndex = 0;

    // @@ Use the api_dump to document the results of the above two
    // step.  How many queue families does your Vulkan offer.  Which
    // of them, by index, has the above three required flags?

    //@@ Search the list for (the index of) the first queue family that has the required flags.
    // Verity that your search choose the correct queue family.
    // Record the index in m_graphicsQueueIndex.
    // Nothing to destroy as m_graphicsQueueIndex is just an integer.
    //m_graphicsQueueIndex = you chosen index;
}


void VkApp::createDevice()
{
    // @@
    // Build a pNext chain of the following six "feature" structures:
    //   features2->features11->features12->features13->accelFeature->rtPipelineFeature->NULL

    // Hint: Keep it simple; add a second parameter (the pNext pointer) to each
    // structure point up to the previous structure.
    
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, NULL};
    
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, &rtPipelineFeature};
    
    VkPhysicalDeviceVulkan13Features features13{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, &accelFeature};
    
    VkPhysicalDeviceVulkan12Features features12{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &features13};
    
    VkPhysicalDeviceVulkan11Features features11{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, &features12};
    
    VkPhysicalDeviceFeatures2 features2{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &features11};

    // Fill in all structures on the pNext chain
    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features2);
    // @@
    // Document the whole filled in pNext chain using an api_dump
    // Examine all the many features.  Do any of them look familiar?

    // Turn off robustBufferAccess (WHY?)
    features2.features.robustBufferAccess = VK_FALSE;

    float priority = 1.0;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = m_graphicsQueueIndex;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &priority;
    
    VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.pNext            = &features2; // This is the whole pNext chain
  
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos    = &queueInfo;
    
    deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(reqDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = reqDeviceExtensions.data();

    const VkResult success = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device);
    // @@
    // Verify VK_SUCCESS
    if (success != VK_SUCCESS)
        printf("Failed to create Device!\n");
	// To destroy: vkDestroyDevice(m_device, nullptr);
}

void VkApp::getCommandQueue()
{
    vkGetDeviceQueue(m_device, m_graphicsQueueIndex, 0, &m_queue);
    // Returns void -- nothing to verify
    // Nothing to destroy -- the queue is owned by the device.
}

// Calling load_VK_EXTENSIONS from extensions_vk.cpp.  A Python script
// from NVIDIA created extensions_vk.cpp from the current Vulkan spec
// for the purpose of loading the symbols for all registered
// extension.  This be (indistinguishable from) magic.
void VkApp::loadExtensions()
{
    load_VK_EXTENSIONS(m_instance, vkGetInstanceProcAddr, m_device, vkGetDeviceProcAddr);
}

//  VkSurface is Vulkan's name for the screen.  Since GLFW creates and
//  manages the window, it creates the VkSurface at our request.
void VkApp::getSurface()
{
    VkBool32 isSupported;   // Supports drawing(presenting) on a screen
    VkResult success;

    // @@ Verify VK_SUCCESS from both the glfw... and the vk... calls.
    success = glfwCreateWindowSurface(m_instance, app->GLFW_window, nullptr, &m_surface);
    if (success != VK_SUCCESS)
        printf("\nFailed to Create GLFW Window Surface!\n");
    
    success = vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_graphicsQueueIndex, m_surface, &isSupported);
    if (success != VK_SUCCESS)
        printf("\nFailed to Get Physical Device Surface Support KHR!\n");

    if (isSupported != VK_TRUE)
        printf("\nVulkan does not support presenting on this surface!\n");

    // @@ Verify isSupported==VK_TRUE, meaning that Vulkan supports presenting on this surface.
    //To destroy: vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
}

// Create a command pool, used to allocate command buffers, which in
// turn are use to gather and send commands to the GPU.  The flag
// makes it possible to reuse command buffers.  The queue index
// determines which queue the command buffers can be submitted to.
// Use the command pool to also create a command buffer.
void VkApp::createCommandPool()
{
    VkCommandPoolCreateInfo poolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCreateInfo.queueFamilyIndex = m_graphicsQueueIndex;
    VkResult result = vkCreateCommandPool(m_device, &poolCreateInfo, nullptr, &m_cmdPool);
    if (result != VK_SUCCESS)
        printf("Failed to create command pool!\n");
    // @@ Verify VK_SUCCESS
    // To destroy: vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
    
    // Create a command buffer
    VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool        = m_cmdPool;
    allocateInfo.commandBufferCount = 1;
    allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    result = vkAllocateCommandBuffers(m_device, &allocateInfo, &m_commandBuffer);
    if (result != VK_SUCCESS)
        printf("Failed to allocated command buffers");
    // @@ Verify VK_SUCCESS
    // Nothing to destroy -- the pool owns the command buffer.
}
 
// 
void VkApp::createSwapchain()
{
    VkResult       err;
    VkSwapchainKHR oldSwapchain = m_swapchain;

    vkDeviceWaitIdle(m_device);  // Probably unnecessary

    // Get the surface's capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities);

    // @@  Roll your own two step process to retrieve a list of present mode into
    //  by making two calls to
    //    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, ...);
    // For an example, search above for vkGetPhysicalDeviceQueueFamilyProperties
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data());
    
    // @@ Document your preset modes. I especially want to know if
    // your system offers VK_PRESENT_MODE_MAILBOX_KHR mode.  My
    // high-end windows desktop does; My higher-end Linux laptop
    // doesn't.

    // Choose VK_PRESENT_MODE_FIFO_KHR as a default (this must be supported)
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR; // Support is required.
    // @@ But choose VK_PRESENT_MODE_MAILBOX_KHR if it can be found in
    // the retrieved presentModes. Several Vulkan tutorials opine that
    // MODE_MAILBOX is the premier mode, but this may not be best for
    // us -- expect more about this later.
  

    // Get the list of VkFormat's that are supported:
    //@@ Do the two step process to retrieve a list of surface formats in
    //   std::vector<VkSurfaceFormatKHR> formats;
    // with two calls to
    //   vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, ...);
    uint32_t surfaceFormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &surfaceFormatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(surfaceFormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &surfaceFormatCount, formats.data());
    // @@ Document the list you get.

    VkFormat surfaceFormat       = formats[0].format;
    VkColorSpaceKHR surfaceColor = formats[0].colorSpace; 
    // @@ Replace the above two temporary lines with the following two
    // to choose the first format and its color space as defaults:
    //  VkFormat surfaceFormat = formats[0].format;
    //  VkColorSpaceKHR surfaceColor  = formats[0].colorSpace;

    // @@ Then search the formats (from several lines up) to choose
    // format VK_FORMAT_B8G8R8A8_UNORM (and its color space) if such
    // exists.  Document your list of formats/color-spaces, and your
    // particular choice.
    
    // Get the swap chain extent
    VkExtent2D swapchainExtent = capabilities.currentExtent;
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        swapchainExtent = capabilities.currentExtent; }
    else {
        // Does this case ever happen?
        int width, height;
        glfwGetFramebufferSize(app->GLFW_window, &width, &height);

        swapchainExtent = VkExtent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        swapchainExtent.width = std::clamp(swapchainExtent.width,
                                           capabilities.minImageExtent.width,
                                           capabilities.maxImageExtent.width);
        swapchainExtent.height = std::clamp(swapchainExtent.height,
                                            capabilities.minImageExtent.height,
                                            capabilities.maxImageExtent.height);
    }

    // Test against valid size, typically hit when windows are minimized.
    // The app must prevent triggering this code in such a case
    assert(swapchainExtent.width && swapchainExtent.height);
    // @@ If this assert fires, we have some work to do to better deal
    // with the situation.

    // Choose the number of swap chain images, within the bounds supported.
    uint imageCount = capabilities.minImageCount + 1; // Recommendation: minImageCount+1
    if (capabilities.maxImageCount > 0
        && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount; }
    
    assert (imageCount == 3);
    // If this triggers, disable the assert, BUT help me understand
    // the situation that caused it.  

    // Create the swap chain
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                 | VK_IMAGE_USAGE_STORAGE_BIT
                                 | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
    VkSwapchainCreateInfoKHR _i = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    _i.surface                  = m_surface;
    _i.minImageCount            = imageCount;
    _i.imageFormat              = surfaceFormat;
    _i.imageColorSpace          = surfaceColor;
    _i.imageExtent              = swapchainExtent;
    _i.imageUsage               = imageUsage;
    _i.preTransform             = capabilities.currentTransform;
    _i.compositeAlpha           = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    _i.imageArrayLayers         = 1;
    _i.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;
    _i.queueFamilyIndexCount    = 1;
    _i.pQueueFamilyIndices      = &m_graphicsQueueIndex;
    _i.presentMode              = swapchainPresentMode;
    _i.oldSwapchain             = oldSwapchain;
    _i.clipped                  = true;

    err = vkCreateSwapchainKHR(m_device, &_i, nullptr, &m_swapchain);
    if (err != VK_SUCCESS)
        printf("Failed to create Swapchain!\n");
    // @@ Verify VK_SUCCESS
    
    //@@ Do the two step process to retrieve the list of (3) swapchain images
    //   m_swapchainImages (of type std::vector<VkImage>)
    // with two calls to
    //   vkGetSwapchainImagesKHR(m_device, m_swapchain, ...);
    // Verify success
    // Verify and document that you retrieved the correct number of images.
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, nullptr);
    m_swapchainImages.resize(m_imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, m_swapchainImages.data());
    
    m_barriers.resize(m_imageCount);
    m_imageViews.resize(m_imageCount);

    // Create an VkImageView for each swap chain image.
    for (uint i=0;  i<m_imageCount;  i++) {
        VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            createInfo.image = m_swapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = surfaceFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            err = vkCreateImageView(m_device, &createInfo, nullptr, &m_imageViews[i]);
            if (err != VK_SUCCESS)
                printf("Failed to create ImageView!\n");
    }

    // Create three VkImageMemoryBarrier structures (one for each swap
    // chain image) and specify the desired
    // layout (VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) for each.
    for (uint i=0;  i<m_imageCount;  i++) {
        VkImageSubresourceRange range = {0};
        range.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel            = 0;
        range.levelCount              = VK_REMAINING_MIP_LEVELS;
        range.baseArrayLayer          = 0;
        range.layerCount              = VK_REMAINING_ARRAY_LAYERS;
        
        VkImageMemoryBarrier memBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        memBarrier.dstAccessMask        = 0;
        memBarrier.srcAccessMask        = 0;
        memBarrier.oldLayout            = VK_IMAGE_LAYOUT_UNDEFINED;
        memBarrier.newLayout            = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        memBarrier.image                = m_swapchainImages[i];
        memBarrier.subresourceRange     = range;
        m_barriers[i] = memBarrier;
    }

    // Create a temporary command buffer. submit the layout conversion
    // command, submit and destroy the command buffer.
    VkCommandBuffer cmd = createTempCmdBuffer();
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, m_imageCount, m_barriers.data());
    submitTempCmdBuffer(cmd);

    // Create the three synchronization objects.  These are not
    // technically part of the swap chain, but they are used
    // exclusively for synchronizing the swap chain, so I include them
    // here.
    VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_waitFence);
    
    VkSemaphoreCreateInfo semCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vkCreateSemaphore(m_device, &semCreateInfo, nullptr, &m_readSemaphore);
    vkCreateSemaphore(m_device, &semCreateInfo, nullptr, &m_writtenSemaphore);
    //NAME(m_readSemaphore, VK_OBJECT_TYPE_SEMAPHORE, "m_readSemaphore");
    //NAME(m_writtenSemaphore, VK_OBJECT_TYPE_SEMAPHORE, "m_writtenSemaphore");
    //NAME(m_queue, VK_OBJECT_TYPE_QUEUE, "m_queue");
        
    windowSize = swapchainExtent;
    // To destroy:  Complete and call function destroySwapchain
}

void VkApp::destroySwapchain()
{
    vkDeviceWaitIdle(m_device);

    // @@
    // Destroy all (3)  m_imageView'Ss with
    for (int i = 0; i < 3; ++i)
		vkDestroyImageView(m_device, m_imageViews[i], nullptr);

    // Destroy the synchronization items: 
	vkDestroyFence(m_device, m_waitFence, nullptr);
	vkDestroySemaphore(m_device, m_readSemaphore, nullptr);
	vkDestroySemaphore(m_device, m_writtenSemaphore, nullptr);

    // Destroy the actual swapchain:
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    m_swapchain = VK_NULL_HANDLE;
    m_imageViews.clear();
    m_barriers.clear();
}



void VkApp::createDepthResource() 
{
    uint mipLevels = 1;

    // Note m_depthImage is type ImageWrap; a tiny wrapper around
    // several related Vulkan objects.
    m_depthImage = createImageWrap(windowSize.width, windowSize.height,
                                    VK_FORMAT_X8_D24_UNORM_PACK32,
                                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                    mipLevels);
    m_depthImage.imageView = createImageView(m_depthImage.image,
                                             VK_FORMAT_X8_D24_UNORM_PACK32,
                                             VK_IMAGE_ASPECT_DEPTH_BIT);
    // To destroy: m_depthImage.destroy(m_device);
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
uint32_t VkApp::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i))
            && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i; } }

    throw std::runtime_error("failed to find suitable memory type!");
}


// A factory function for an ImageWrap, this creates a VkImage and
// creates and binds an associated VkDeviceMemory object.  The
// VkImageView and VkSampler are left empty to be created elsewhere as
// needed.
ImageWrap VkApp::createImageWrap(uint32_t width, uint32_t height,
                                 VkFormat format,
                                 VkImageUsageFlags usage,
                                 VkMemoryPropertyFlags properties, uint mipLevels)
{
    ImageWrap myImage;
    
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateImage(m_device, &imageInfo, nullptr, &myImage.image);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, myImage.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    vkAllocateMemory(m_device, &allocInfo, nullptr, &myImage.memory);
    
    vkBindImageMemory(m_device, myImage.image, myImage.memory, 0);

    myImage.imageView = VK_NULL_HANDLE;
    myImage.sampler = VK_NULL_HANDLE;

    return myImage;
    // @@ Verify success for vkCreateImage, and vkAllocateMemory
}

VkImageView VkApp::createImageView(VkImage image, VkFormat format,
                                         VkImageAspectFlagBits aspect)
{
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    VkImageView imageView;
    vkCreateImageView(m_device, &viewInfo, nullptr, &imageView);
    // @@ Verify success for vkCreateImageView
    
    return imageView;
}

void VkApp::createPostRenderPass()
{  
    std::array<VkAttachmentDescription, 2> attachments{};
    // Color attachment
    attachments[0].format      = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[0].loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[0].samples     = VK_SAMPLE_COUNT_1_BIT;

    // Depth attachment
    attachments[1].format        = VK_FORMAT_X8_D24_UNORM_PACK32;
    attachments[1].loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].samples       = VK_SAMPLE_COUNT_1_BIT;

    const VkAttachmentReference colorReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference depthReference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};


    std::array<VkSubpassDependency, 1> subpassDependencies{};
    // Transition from final to initial (VK_SUBPASS_EXTERNAL refers to all commands executed outside of the actual renderpass)
    subpassDependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].dstSubpass      = 0;
    subpassDependencies[0].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkSubpassDescription subpassDescription{};
    subpassDescription.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount    = 1;
    subpassDescription.pColorAttachments       = &colorReference;
    subpassDescription.pDepthStencilAttachment = &depthReference;

    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpassDescription;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderPassInfo.pDependencies   = subpassDependencies.data();

    VkResult result = vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_postRenderPass);
    if (result != VK_SUCCESS)
        printf("Failed to create render pass!\n");
    // To destroy: vkDestroyRenderPass(m_device, m_postRenderPass, nullptr);
}

// A VkFrameBuffer wraps several images into a render target --
// usually a color buffer and a depth buffer.
void VkApp::createPostFrameBuffers()
{
    std::array<VkImageView, 2> fbattachments{};
    
    // Create frame buffers for every swap chain image
    VkFramebufferCreateInfo _ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    _ci.renderPass      = m_postRenderPass;
    _ci.width           = windowSize.width;
    _ci.height          = windowSize.height;
    _ci.layers          = 1;
    _ci.attachmentCount = 2;
    _ci.pAttachments    = fbattachments.data();

    // Each of the three swapchain images gets an associated frame
    // buffer, all sharing one depth buffer.
    m_framebuffers.resize(m_imageCount);
    for(uint32_t i = 0; i < m_imageCount; i++) {
        fbattachments[0] = m_imageViews[i];         // A color attachment from the swap chain
        fbattachments[1] = m_depthImage.imageView;  // A depth attachment
        vkCreateFramebuffer(m_device, &_ci, nullptr, &m_framebuffers[i]); }

    // To destroy: In a loop, call: vkDestroyFramebuffer(m_device, m_framebuffers[i], nullptr);
    // Verify success
}


void VkApp::createPostPipeline()
{

    // Creating the pipeline layout
    VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};


    // What we eventually want:
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &m_postDesc.descSetLayout;
    // What we can do for now:
    /*createInfo.setLayoutCount         = 0;
    createInfo.pSetLayouts            = nullptr;*/
    
    createInfo.pushConstantRangeCount = 0;
    createInfo.pPushConstantRanges    = nullptr;
    
    vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_postPipelineLayout);

    ////////////////////////////////////////////
    // Create the shaders
    ////////////////////////////////////////////
    VkShaderModule vertShaderModule = createShaderModule(loadFile("spv/post.vert.spv"));
    VkShaderModule fragShaderModule = createShaderModule(loadFile("spv/post.frag.spv"));

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    //auto bindingDescription = Vertex::getBindingDescription();
    //auto attributeDescriptions = Vertex::getAttributeDescriptions();

    // No geometry in this pipeline's draw.
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
        
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) windowSize.width;
    viewport.height = (float) windowSize.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = VkExtent2D{windowSize.width, windowSize.height};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE; //??
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;// BEWARE!!  NECESSARY!!
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = m_postPipelineLayout;
    pipelineInfo.renderPass = m_postRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkResult result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_postPipeline);
    if (result != VK_SUCCESS)
        printf("Failed to Create Graphics Pipelines!\n");

    // The pipeline has fully compiled copies of the shaders, so these
    // intermediate (SPV) versions can be destroyed.
    // @@
    // For the two modules fragShaderModule and vertShaderModule
    // destroy right *here* via vkDestroyShaderModule(m_device, ..., nullptr);

    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    
    // To destroy:  vkDestroyPipelineLayout(m_device, m_postPipelineLayout, nullptr);
    //  and:        vkDestroyPipeline(m_device, m_postPipeline, nullptr);
    // Document the vkCreateGraphicsPipelines call with an api_dump.  

}

std::string VkApp::loadFile(const std::string& filename)
{
    std::string   result;
    std::ifstream stream(filename, std::ios::ate | std::ios::binary);  //ate: Open at file end

    if(!stream.is_open())
        return result;

    result.reserve(stream.tellg()); // tellg() is last char position in file (i.e.,  length)
    stream.seekg(0, std::ios::beg);

    result.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    return result;
}

//-------------------------------------------------------------------------------------------------
// Post processing pass: tone mapper, UI
void VkApp::postProcess()
{
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{1,1,1,1}};
    clearValues[1].depthStencil = {1.0f, 0};
            
    VkRenderPassBeginInfo _i{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    _i.clearValueCount = 2;
    _i.pClearValues    = clearValues.data();
    _i.renderPass      = m_postRenderPass;
    _i.framebuffer     = m_framebuffers[m_swapchainIndex];
    _i.renderArea      = {{0, 0}, windowSize};
    
    vkCmdBeginRenderPass(m_commandBuffer, &_i, VK_SUBPASS_CONTENTS_INLINE);
    {   // extra indent for renderpass commands
        
        auto aspectRatio = static_cast<float>(windowSize.width)
            / static_cast<float>(windowSize.height);
        vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postPipeline);
        // Eventually uncomment this
        vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_postPipelineLayout, 0, 1, &m_postDesc.descSet, 0, nullptr);

        // Weird! This draws 3 vertices but with no vertices/triangles buffers bound in.
        // Hint: The vertex shader fabricates vertices from gl_VertexIndex
        vkCmdDraw(m_commandBuffer, 3, 1, 0, 0);

        #ifdef GUI
        ImGui::Render();  // Rendering UI
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffer);
        #endif
    }
    vkCmdEndRenderPass(m_commandBuffer);
}
