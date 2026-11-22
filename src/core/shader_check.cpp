#include "core/shader_check.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace rtrt
{

namespace
{

constexpr const char* kShaderNames[] = {
    "post.vert",     "post.frag",     "scanline.vert",  "scanline.frag",
    "denoise.comp",  "raytrace.rgen", "raytrace.rmiss", "raytraceShadow.rmiss",
    "raytrace.rchit"};

std::vector<std::string> MissingShaders(bool Slang, bool Glsl)
{
    std::vector<std::string> Missing;
    for (const char* Name : kShaderNames)
    {
        const std::string SlangSpv = std::string("spv/") + Name + ".slang.spv";
        const std::string GlslSpv = std::string("spv/") + Name + ".spv";
        if (Slang && !std::filesystem::exists(SlangSpv))
        {
            Missing.push_back(SlangSpv);
        }
        if (Glsl && !std::filesystem::exists(GlslSpv))
        {
            Missing.push_back(GlslSpv);
        }
    }
    return Missing;
}

}  // namespace

bool GlslShadersBuilt()
{
    return MissingShaders(false, true).empty();
}

bool CompileShaders(bool WithGlsl)
{
    fflush(stdout);
#ifdef _WIN32
    return std::system(WithGlsl ? "..\\scripts\\compile_shaders.bat glsl"
                                : "..\\scripts\\compile_shaders.bat") == 0;
#else
    fprintf(stderr, "Compiling shaders is only set up on Windows\n");
    return false;
#endif
}

bool EnsureShaders(bool WithGlsl)
{
    const std::vector<std::string> Missing = MissingShaders(true, WithGlsl);
    if (Missing.empty())
    {
        return true;
    }

    printf("%zu compiled shaders are missing from spv/, compiling them\n",
           Missing.size());
    CompileShaders(WithGlsl);

    const std::vector<std::string> StillMissing = MissingShaders(true, false);
    for (const std::string& Path : StillMissing)
    {
        fprintf(stderr, "Missing shader: %s\n", Path.c_str());
    }
    return StillMissing.empty();
}

}  // namespace rtrt
