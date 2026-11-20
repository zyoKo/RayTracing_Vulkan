
#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shared_structs.h"

layout(location = 0) out vec4 fragColor;
layout(set=0, binding=0) uniform sampler2D renderedImage;

layout(push_constant) uniform _PushConstantPost { PushConstantPost pc; };

vec3 gammaEncode(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(1.0/pc.Gamma));
}

void main()
{
    vec4 value = texelFetch(renderedImage, ivec2(gl_FragCoord.xy), 0);

    vec3 color;
    if (pc.ViewMode == kViewAlbedo)
        color = gammaEncode(value.xyz);
    else if (pc.ViewMode == kViewNormal)
        color = value.xyz * 0.5 + 0.5;
    else if (pc.ViewMode == kViewDepth)
        color = vec3(value.w / 15.0);
    else if (pc.ViewMode == kViewSamples)
        color = vec3(log2(1.0 + value.w) / 12.0);  // White at 4095 samples
    else
        color = gammaEncode(value.xyz * pc.Exposure);

    fragColor = vec4(color, 1.0);
}
