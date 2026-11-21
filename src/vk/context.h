#ifndef RTRT_SRC_VK_CONTEXT_H_
#define RTRT_SRC_VK_CONTEXT_H_

#include <vector>

#include <vulkan/vulkan_core.h>

#include "core/window.h"

namespace rtrt
{

// The Vulkan instance, with the validation layer on, and the window's surface.
class Context
{
public:
    Context() = default;
    ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    // Creates the instance and the surface for Target. DoApiDump adds the api_dump
    // layer in front of the validation layer.
    bool Init(const Window& Target, bool DoApiDump);

    VkInstance GetInstance() const
    {
        return Instance;
    }

    VkSurfaceKHR GetSurface() const
    {
        return Surface;
    }

private:
    void CreateInstance(bool DoApiDump);

    std::vector<const char*> ReqInstanceExtensions = {
        "VK_EXT_debug_utils", "VK_KHR_win32_surface", "VK_KHR_surface"};
    std::vector<const char*> ReqInstanceLayers = {"VK_LAYER_KHRONOS_validation"};

    VkInstance Instance = VK_NULL_HANDLE;
    VkSurfaceKHR Surface = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_CONTEXT_H_
