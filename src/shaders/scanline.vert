#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shared_structs.h"

layout(binding = 0) uniform _MatrixUniforms
{
  MatrixUniforms mats;
};

layout(push_constant) uniform _PushConstantRaster
{
  PushConstantRaster pcRaster;
};

layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texCoord;


layout(location = 1) out vec3 worldPos;
layout(location = 2) out vec3 worldNrm;
layout(location = 3) out vec3 viewDir;
layout(location = 4) out vec2 texCoord;

out gl_PerVertex
{
  vec4 gl_Position;
};


void main()
{
  vec3 eye = vec3(mats.viewInverse * vec4(0, 0, 0, 1));

  worldPos = vec3(pcRaster.modelMatrix * vec4(i_position, 1.0));
  viewDir  = vec3(eye - worldPos);
  texCoord = i_texCoord;
  worldNrm = mat3(pcRaster.modelMatrix) * i_normal;

  gl_Position = mats.viewProj * vec4(worldPos, 1.0);
}
