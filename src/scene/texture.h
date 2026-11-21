#ifndef RTRT_SRC_SCENE_TEXTURE_H_
#define RTRT_SRC_SCENE_TEXTURE_H_

#include <string>

#include "vk/command_pool.h"
#include "vk/device.h"
#include "vk/image.h"

namespace rtrt
{

// Loads an image file as an RGBA8 texture with a full mip chain, a view and a sampler,
// ready for shader reads. A file that can't be read is a fatal error.
Image LoadTexture(const Device& Gpu, const CommandPool& Pool,
                  const std::string& FileName);

}  // namespace rtrt

#endif  // RTRT_SRC_SCENE_TEXTURE_H_
