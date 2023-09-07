#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shared_structs.h"

layout(location=0) rayPayloadInEXT RayPayload payload;

void main()
{
    // Intentionally empty;  To be filled in later in the semester
}
