#include "shared_structs.h"

[[vk::binding(0, 0)]] Sampler2D renderedImage;

[[vk::push_constant]] ConstantBuffer<PushConstantPost> pc;

// One triangle covering all of NDC [-1,1]x[-1,1]
static const float3 points[3] = { float3(-1, -1, 0), float3(-1, 3, 0), float3(3, -1, 0) };

[shader("vertex")]
float4 vertexMain(uint vertexId : SV_VulkanVertexID) : SV_Position
{
    return float4(points[vertexId], 1.0);
}

float3 gammaEncode(float3 color)
{
    return pow(max(color, float3(0.0)), float3(1.0 / pc.gamma));
}

[shader("fragment")]
float4 fragmentMain(float4 fragCoord : SV_Position) : SV_Target0
{
    float4 value = renderedImage.Load(int3(int2(fragCoord.xy), 0));

    float3 color;
    if (pc.viewMode == eViewAlbedo)
        color = gammaEncode(value.xyz);
    else if (pc.viewMode == eViewNormal)
        color = value.xyz * 0.5 + 0.5;
    else if (pc.viewMode == eViewDepth)
        color = float3(value.w / 15.0);
    else if (pc.viewMode == eViewSamples)
        color = float3(log2(1.0 + value.w) / 12.0);  // White at 4095 samples
    else
        color = gammaEncode(value.xyz * pc.exposure);

    return float4(color, 1.0);
}
