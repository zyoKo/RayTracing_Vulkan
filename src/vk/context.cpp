#include "vk/context.h"

#include <cstdio>
#include <vector>

#include <GLFW/glfw3.h>

#include "vk_check.h"

namespace rtrt
{

Context::~Context()
{
    if (Surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(Instance, Surface, nullptr);
    }
    if (Instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(Instance, nullptr);
    }
}

bool Context::Init(const Window& Target, bool DoApiDump)
{
    CreateInstance(DoApiDump);
    RTRT_CHECK_VK(
        glfwCreateWindowSurface(Instance, Target.GetHandle(), nullptr, &Surface));
    return true;
}

void Context::CreateInstance(bool DoApiDump)
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

}  // namespace rtrt
