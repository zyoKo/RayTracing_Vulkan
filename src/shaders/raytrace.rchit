#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shared_structs.h"

layout(location=0) rayPayloadInEXT RayPayload payload;

hitAttributeEXT vec2 bc;  // Hit point's barycentric coordinates (two of them)

void main()
{
    payload.hit = true;
    payload.hitDist = gl_HitTEXT;
    payload.instanceIndex = gl_InstanceCustomIndexEXT;
    payload.primitiveIndex = gl_PrimitiveID;
    payload.bc = vec3(1.0-bc.x-bc.y, bc.x, bc.y);
    payload.hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
}
