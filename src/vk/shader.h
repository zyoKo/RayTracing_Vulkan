#ifndef RTRT_SRC_VK_SHADER_H_
#define RTRT_SRC_VK_SHADER_H_

#include <string>

#include <vulkan/vulkan_core.h>

namespace rtrt
{

// A shader module loaded from either the GLSL or the Slang SPIR-V of one shader. Frees
// itself when destroyed; moves hand the handle over.
class Shader
{
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& Other) noexcept;
    Shader& operator=(Shader&& Other) noexcept;

    // Loads SlangSpv when UseSlang is set and the file exists, otherwise GlslSpv.
    void Load(VkDevice LogicalDevice, const char* GlslSpv, const char* SlangSpv,
              bool UseSlang);

    // Safe to call on an empty shader.
    void Destroy();

    // A pipeline stage that runs this shader from EntryPoint.
    VkPipelineShaderStageCreateInfo StageInfo(VkShaderStageFlagBits Stage,
                                              const char* EntryPoint = "main") const;

    VkShaderModule GetHandle() const
    {
        return Handle;
    }

private:
    // The whole file, or an empty string when it can't be opened.
    static std::string LoadFile(const std::string& FileName);

    VkDevice DeviceHandle = VK_NULL_HANDLE;
    VkShaderModule Handle = VK_NULL_HANDLE;
};

}  // namespace rtrt

#endif  // RTRT_SRC_VK_SHADER_H_
