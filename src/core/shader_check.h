#ifndef RTRT_SRC_CORE_SHADER_CHECK_H_
#define RTRT_SRC_CORE_SHADER_CHECK_H_

namespace rtrt
{

// True when every SPIR-V file built from Slang's generated GLSL is in spv/.
bool GlslShadersBuilt();

// Runs compile_shaders.bat, adding the generated-GLSL shaders when WithGlsl is set.
// Blocks until it finishes; true when it succeeded.
bool CompileShaders(bool WithGlsl);

// Makes sure the Slang shaders are in spv/, and the generated-GLSL ones too when
// WithGlsl is set, compiling them when any are missing. False when the Slang shaders
// are still missing afterwards.
bool EnsureShaders(bool WithGlsl);

}  // namespace rtrt

#endif  // RTRT_SRC_CORE_SHADER_CHECK_H_
