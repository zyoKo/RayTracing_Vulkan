#ifndef RTRT_SRC_RENDERER_RENDER_IMAGE_H_
#define RTRT_SRC_RENDERER_RENDER_IMAGE_H_

#include <vulkan/vulkan_core.h>

#include "vk/device.h"
#include "vk/image.h"

namespace rtrt
{

// An RGBA32F image of Size that the passes render into and read from: storage, sampled,
// color attachment and copy source and target, with a view and a sampler. It reports
// the general layout but starts undefined, so the caller moves it there.
Image CreateRenderImage(const Device& Gpu, VkExtent2D Size);

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_RENDER_IMAGE_H_
