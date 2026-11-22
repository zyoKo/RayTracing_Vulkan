#ifndef RTRT_SRC_RENDERER_PASS_CONTEXT_H_
#define RTRT_SRC_RENDERER_PASS_CONTEXT_H_

#include <vulkan/vulkan_core.h>

#include "vk/command_pool.h"
#include "vk/device.h"
#include "vk/swapchain.h"

namespace rtrt
{

// The device objects every pass builds and records with. The Renderer fills it in and
// keeps it current; the pointed-to objects outlive the passes.
struct PassContext
{
    const Device* Gpu = nullptr;
    const CommandPool* Pool = nullptr;
    const Swapchain* Chain = nullptr;
    // Size of every render image; the viewport's size, not the window's
    VkExtent2D Extent{0, 0};
    // Load the Slang SPIR-V instead of the one built from generated GLSL
    bool UseSlang = true;
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_PASS_CONTEXT_H_
