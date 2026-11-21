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

std::vector<std::string> MissingShaders(bool IncludeSlang)
{
    std::vector<std::string> Missing;
    for (const char* Name : kShaderNames)
    {
        const std::string Glsl = std::string("spv/") + Name + ".spv";
        const std::string Slang = std::string("spv/") + Name + ".slang.spv";
        if (!std::filesystem::exists(Glsl))
        {
            Missing.push_back(Glsl);
        }
        if (IncludeSlang && !std::filesystem::exists(Slang))
        {
            Missing.push_back(Slang);
        }
    }
    return Missing;
}

}  // namespace

bool EnsureShaders()
{
    const std::vector<std::string> Missing = MissingShaders(true);
    if (Missing.empty())
    {
        return true;
    }

    printf("%zu compiled shaders are missing from spv/, compiling them\n",
           Missing.size());
    fflush(stdout);
#ifdef _WIN32
    std::system(".\\compile_shaders.bat");
#else
    fprintf(stderr, "Run make to compile the shaders\n");
#endif

    const std::vector<std::string> StillMissing = MissingShaders(false);
    for (const std::string& Path : StillMissing)
    {
        fprintf(stderr, "Missing shader: %s\n", Path.c_str());
    }
    return StillMissing.empty();
}

}  // namespace rtrt
