
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "shared_structs.h"

const vec3 ambientIntensity = vec3(0.2);

layout(push_constant) uniform _PushConstantRaster
{
  PushConstantRaster pcRaster;
};

// clang-format off
// Incoming 
layout(location=1) in vec3 worldPos;
layout(location=2) in vec3 worldNrm;
layout(location=3) in vec3 viewDir;
layout(location=4) in vec2 texCoord;
// Outgoing
layout(location = 0) out vec4 fragColor;

layout(buffer_reference, scalar) buffer Vertices {Vertex v[]; };    // Positions of an object
layout(buffer_reference, scalar) buffer Indices {uint i[]; };       // Triangle indices
layout(buffer_reference, scalar) buffer Materials {Material m[]; }; // Array of materials
layout(buffer_reference, scalar) buffer MatIndices {int i[]; };     // Material ID for each triangle

layout(binding=eObjDescs, scalar) buffer ObjDesc_ { ObjDesc i[]; } objDesc;
layout(binding=eTextures) uniform sampler2D[] textureSamplers;

float pi = 3.14159;
void main()
{
  // Material of the object
  ObjDesc    obj = objDesc.i[pcRaster.objIndex];
  MatIndices matIndices  = MatIndices(obj.materialIndexAddress);
  Materials  materials   = Materials(obj.materialAddress);
  
  int matIndex = matIndices.i[gl_PrimitiveID];
  Material mat = materials.m[matIndex];
  
  vec3 N = normalize(worldNrm);
  vec3 V = normalize(viewDir);
  vec3 lDir = pcRaster.lightPosition - worldPos;
  vec3 L = normalize(lDir);

  float NL = max(dot(N,L),0.0);

  
  vec3 Kd = mat.diffuse;
  vec3 Ks = mat.specular;
  const float alpha = mat.shininess;
  
  if (mat.textureId >= 0)
  {
    int  txtOffset  = obj.txtOffset;
    uint txtId      = txtOffset + mat.textureId;
    Kd = texture(textureSamplers[nonuniformEXT(txtId)], texCoord).xyz;
  }
  
  // This very minimal lighting calculation should be replaced with a modern BRDF calculation. 
  fragColor.xyz = pcRaster.lightIntensity*NL*Kd/pi;

}
