#ifndef RTRT_SRC_RENDERER_PASS_CONTEXT_H_
#define RTRT_SRC_RENDERER_PASS_CONTEXT_H_

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
    // Load the Slang SPIR-V instead of the GLSL
    bool UseSlang = false;
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_PASS_CONTEXT_H_
