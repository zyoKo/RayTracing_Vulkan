#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shared_structs.h"

layout(location=0) rayPayloadInEXT RayPayload payload;

hitAttributeEXT vec2 bc;  // Hit point's barycentric coordinates (two of them)

void main()
{
    payload.Hit = true;
    payload.HitDist = gl_HitTEXT;
    payload.InstanceIndex = gl_InstanceCustomIndexEXT;
    payload.PrimitiveIndex = gl_PrimitiveID;
    payload.Bc = vec3(1.0-bc.x-bc.y, bc.x, bc.y);
    payload.HitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
}
