// One A-trous wavelet pass, edge-stopped by depth, normal and albedo.

#include "shared_structs.h"

static const int GROUP_SIZE = 128;  // Must match GROUP_SIZE in renderer_denoise.cpp

[[vk::binding(0, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> inImage;
[[vk::binding(1, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> outImage;
[[vk::binding(2, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> kdBuff;
[[vk::binding(3, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> ndBuff;

[[vk::push_constant]] ConstantBuffer<PushConstantDenoise> pc;

static const float gaussian[5] = { 1.0 / 16.0, 4.0 / 16.0, 6.0 / 16.0, 4.0 / 16.0, 1.0 / 16.0 };

[shader("compute")]
[numthreads(GROUP_SIZE, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    int2 gpos = int2(dispatchId.xy);  // Index of central pixel being denoised

    // Central pixel: albedo (clamped so demodulation stays stable), color,
    // demodulated color, normal and depth
    float3 cKd = clamp(kdBuff.Load(gpos).xyz, float3(0.1), float3(1.0));
    float3 cVal = inImage.Load(gpos).xyz;
    float3 cDem = cVal / cKd;
    float4 cNd = ndBuff.Load(gpos);
    float3 cNrm = cNd.xyz;
    float cDepth = cNd.w;

    float3 numerator = float3(0.0);
    float denominator = 0.0;
    for (int i = -2; i <= 2; i++)
    {
        for (int j = -2; j <= 2; j++)
        {
            // Offset of 5x5 pixels **with holes**
            int2 pos = gpos + int2(i, j) * pc.StepWidth;
            float3 pKd = clamp(kdBuff.Load(pos).xyz, float3(0.1), float3(1.0));
            float3 pVal = inImage.Load(pos).xyz;
            float3 pDem = pVal / pKd;
            float4 pNd = ndBuff.Load(pos);
            float3 pNrm = pNd.xyz;
            float pDepth = pNd.w;

            // The weight compares the offset pixel to the central one and is a product of:
            //  1-2: separable Gaussian taps in x and y
            float h_weight = gaussian[i + 2];
            float v_weight = gaussian[j + 2];
            //  3: depth similarity, exp(-t^2 / depthFactor) (1 if factor is 0)
            float t = cDepth - pDepth;
            float d_weight = pc.DepthFactor == 0.0 ? 1.0 : exp(-(t * t) / pc.DepthFactor);
            //  4: normal similarity, exp(-|cNrm - pNrm|^2 / stepwidth^2 / normFactor) (1 if factor is 0)
            float3 nDiff = cNrm - pNrm;
            float d = dot(nDiff, nDiff) / float(pc.StepWidth * pc.StepWidth);
            float n_weight = pc.NormFactor == 0.0 ? 1.0 : exp(-d / pc.NormFactor);
            //  5: albedo similarity; keeps specular light on dark-albedo pixels
            //     from bleeding onto brighter neighbours as a halo.
            float3 kdDiff = cKd - pKd;
            float k_weight = pc.AlbedoFactor == 0.0 ? 1.0 : exp(-dot(kdDiff, kdDiff) / pc.AlbedoFactor);

            float weight = h_weight * v_weight * d_weight * n_weight * k_weight;
            numerator += pDem * weight;
            denominator += weight;
        }
    }

    // Re-modulate the color
    float3 outVal = denominator == 0.0 ? cVal : cKd * numerator / denominator;

    outImage.Store(gpos, float4(outVal, 0.0));
}
