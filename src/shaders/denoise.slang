// One A-trous wavelet pass, edge-stopped by depth, normal and albedo.

#include "shared_structs.h"

static const int kGroupSize = 128;  // Must match kGroupSize in renderer_denoise.cpp

[[vk::binding(0, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> inImage;
[[vk::binding(1, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> outImage;
[[vk::binding(2, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> kdBuff;
[[vk::binding(3, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> ndBuff;

[[vk::push_constant]] ConstantBuffer<PushConstantDenoise> pc;

static const float gaussian[5] = { 1.0 / 16.0, 4.0 / 16.0, 6.0 / 16.0, 4.0 / 16.0, 1.0 / 16.0 };

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

// Luminance mapped to [0, 1) so a single very bright sample can't dominate the colour weight
float CompressedLuminance(float3 c)
{
    float l = Luminance(c);
    return l / (1.0 + l);
}

[shader("compute")]
[numthreads(kGroupSize, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    int2 gpos = int2(dispatchId.xy);  // Index of central pixel being denoised

    // Central pixel: albedo (clamped so demodulation stays stable), color,
    // demodulated color, normal and depth
    float4 cKdRaw = kdBuff.Load(gpos);
    float4 cIn = inImage.Load(gpos);
    float3 cVal = cIn.xyz;
    float cSamples = cIn.w;

    // Light sources (Kd.w = 1) are already noise free; filtering them would only spread them
    if (cKdRaw.w > 0.5)
    {
        outImage.Store(gpos, float4(cVal, cSamples));
        return;
    }

    float3 cKd = clamp(cKdRaw.xyz, float3(0.1), float3(1.0));
    float3 cDem = cVal / cKd;

    // Outlier clamp: on the first pass, keep the centre's luminance within the range of its
    // 8 neighbours, which removes lone fireflies and lone black pixels
    if (pc.ClampFireflies != 0 && pc.StepWidth == 1)
    {
        float maxLum = -1.0;
        float minLum = 1e30;
        float3 minDem = float3(0.0);
        for (int i = -1; i <= 1; i++)
        {
            for (int j = -1; j <= 1; j++)
            {
                if (i == 0 && j == 0)
                    continue;
                int2 q = gpos + int2(i, j);
                float4 qKd = kdBuff.Load(q);
                if (qKd.w > 0.5)
                    continue;
                float3 qDem = inImage.Load(q).xyz / clamp(qKd.xyz, float3(0.1), float3(1.0));
                float qLum = Luminance(qDem);
                maxLum = max(maxLum, qLum);
                if (qLum < minLum)
                {
                    minLum = qLum;
                    minDem = qDem;
                }
            }
        }
        float lum = Luminance(cDem);
        if (maxLum >= 0.0 && lum > maxLum)
            cDem *= maxLum / lum;
        else if (maxLum >= 0.0 && lum < minLum)
            cDem = minDem;
    }

    // A pixel with few samples (just revealed by camera motion) is too noisy to trust its
    // own colour, so the colour weight fades in as samples accumulate
    float colorTrust = saturate(cSamples / 16.0);

    float cLum = CompressedLuminance(cDem);
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
            float4 pKdRaw = kdBuff.Load(pos);
            if (pKdRaw.w > 0.5)
                continue;
            float3 pKd = clamp(pKdRaw.xyz, float3(0.1), float3(1.0));
            float3 pVal = inImage.Load(pos).xyz;
            float3 pDem = pVal / pKd;
            if (i == 0 && j == 0)
                pDem = cDem;
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

            //  6: colour similarity on compressed luminance (1 if factor is 0). The factor
            //     halves each pass, so the wide passes don't smear noise into blocks.
            float l = cLum - CompressedLuminance(pDem);
            float c_weight = pc.ColorFactor == 0.0 ? 1.0 : lerp(1.0, exp(-(l * l) / pc.ColorFactor), colorTrust);

            float weight = h_weight * v_weight * d_weight * n_weight * k_weight * c_weight;
            numerator += pDem * weight;
            denominator += weight;
        }
    }

    // Re-modulate the color
    float3 outVal = denominator == 0.0 ? cVal : cKd * numerator / denominator;

    outImage.Store(gpos, float4(outVal, cSamples));
}
