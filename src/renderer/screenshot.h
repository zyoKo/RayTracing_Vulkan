#ifndef RTRT_SRC_RENDERER_SCREENSHOT_H_
#define RTRT_SRC_RENDERER_SCREENSHOT_H_

#include <cstdint>
#include <string>

#include <vulkan/vulkan_core.h>

#include "vk/buffer.h"
#include "vk/device.h"
#include "vk/image.h"

namespace rtrt
{

// Saves the viewport image as a PNG: the copy is recorded into the frame's command
// buffer and the file is written after that frame's fence, so nothing stalls.
class Screenshot
{
public:
    // The next recorded frame is saved to Path; its folder is created if missing.
    void Request(const std::string& Path)
    {
        RequestedPath = Path;
    }

    // Copies Source, an RGBA8 image in the shader-read layout, when a save is requested.
    void Record(VkCommandBuffer Cmd, const Device& Gpu, const Image& Source,
                VkExtent2D Size);

    // Writes the file for the last recorded copy. Call once its fence has signalled.
    void Resolve();

    void Destroy();

    // The path written, or why it failed, for the last finished save.
    const std::string& GetLastResult() const
    {
        return LastResult;
    }

    bool GetLastSucceeded() const
    {
        return LastSucceeded;
    }

    // Goes up by one for every finished save.
    uint32_t GetSaveCount() const
    {
        return SaveCount;
    }

private:
    std::string RequestedPath;
    std::string InFlightPath;
    Buffer Staging;
    VkExtent2D InFlightSize{0, 0};
    std::string LastResult;
    bool LastSucceeded = false;
    uint32_t SaveCount = 0;
};

}  // namespace rtrt

#endif  // RTRT_SRC_RENDERER_SCREENSHOT_H_
