#ifndef RTRT_SRC_CORE_SHADER_CHECK_H_
#define RTRT_SRC_CORE_SHADER_CHECK_H_

namespace rtrt
{

// Makes sure the compiled shaders are in spv/, running compile_shaders.bat when any
// are missing. False when the Slang shaders are still missing afterwards; the
// generated-GLSL ones are optional.
bool EnsureShaders();

}  // namespace rtrt

#endif  // RTRT_SRC_CORE_SHADER_CHECK_H_
