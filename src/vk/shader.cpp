#include "vk/shader.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

#include "vk_check.h"

namespace rtrt
{

Shader::~Shader()
{
    Destroy();
}

Shader::Shader(Shader&& Other) noexcept
    : DeviceHandle(std::exchange(Other.DeviceHandle, VK_NULL_HANDLE)),
      Handle(std::exchange(Other.Handle, VK_NULL_HANDLE))
{
}

Shader& Shader::operator=(Shader&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        DeviceHandle = std::exchange(Other.DeviceHandle, VK_NULL_HANDLE);
        Handle = std::exchange(Other.Handle, VK_NULL_HANDLE);
    }
    return *this;
}

void Shader::Load(VkDevice LogicalDevice, const char* GlslSpv, const char* SlangSpv,
                  bool UseSlang)
{
    Destroy();
    DeviceHandle = LogicalDevice;

    const char* Primary = UseSlang ? SlangSpv : GlslSpv;
    const char* Fallback = UseSlang ? GlslSpv : SlangSpv;
    std::string Code = LoadFile(Primary);
    if (Code.empty())
    {
        printf("Shader %s not found; using %s\n", Primary, Fallback);
        Code = LoadFile(Fallback);
    }

    VkShaderModuleCreateInfo CreateInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    CreateInfo.codeSize = Code.size();
    CreateInfo.pCode = reinterpret_cast<const uint32_t*>(Code.data());
    RTRT_CHECK_VK(vkCreateShaderModule(DeviceHandle, &CreateInfo, nullptr, &Handle));
}

void Shader::Destroy()
{
    if (DeviceHandle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyShaderModule(DeviceHandle, Handle, nullptr);
    Handle = VK_NULL_HANDLE;
    DeviceHandle = VK_NULL_HANDLE;
}

VkPipelineShaderStageCreateInfo Shader::StageInfo(VkShaderStageFlagBits Stage,
                                                  const char* EntryPoint) const
{
    VkPipelineShaderStageCreateInfo ShaderStage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    ShaderStage.stage = Stage;
    ShaderStage.module = Handle;
    ShaderStage.pName = EntryPoint;
    return ShaderStage;
}

std::string Shader::LoadFile(const std::string& FileName)
{
    std::string Result;
    // Opened at the end, so tellg() is the file size
    std::ifstream Stream(FileName, std::ios::ate | std::ios::binary);
    if (!Stream.is_open())
    {
        return Result;
    }

    Result.reserve(static_cast<size_t>(Stream.tellg()));
    Stream.seekg(0, std::ios::beg);
    Result.assign((std::istreambuf_iterator<char>(Stream)),
                  std::istreambuf_iterator<char>());
    return Result;
}

}  // namespace rtrt
