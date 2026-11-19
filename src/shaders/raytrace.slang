#include "shared_structs.h"
#include "rng.slang"

static const float PI = 3.14159265f;

[[vk::push_constant]] ConstantBuffer<PushConstantRay> pcRay;

[[vk::binding(0, 0)]] RaytracingAccelerationStructure topLevelAS;
[[vk::binding(1, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> colCurr;
[[vk::binding(2, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> colPrev;
[[vk::binding(3, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> NdCurr;
[[vk::binding(4, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> NdPrev;
[[vk::binding(5, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> KdCurr;
[[vk::binding(6, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> KdPrev;

[[vk::binding(eMatrices, 1)]] ConstantBuffer<MatrixUniforms> mats;
[[vk::binding(eObjDescs, 1)]] StructuredBuffer<ObjDesc> objDesc;
[[vk::binding(eTextures, 1)]] Sampler2D textureSamplers[];

// Smith masking term for one direction (Walter et al. 2007). dotVN must be > 0.
float G1(float dotVN, float dotVH, float alphaSqr)
{
    if (dotVH <= 0.0f)
        return 0.0f;

    float cosSqr = dotVN * dotVN;
    float tanSqr = (1.0f - cosSqr) / cosSqr;
    return 2.0f / (1.0f + sqrt(1.0f + alphaSqr * tanSqr));
}

float3 EvalBrdf(float3 N, float3 L, float3 V, Material mat)
{
    float dotLN = dot(L, N);
    float dotVN = dot(V, N);
    if (dotLN <= 0.0f || dotVN <= 0.0f)
        return float3(0.0f);

    float3 Kd = mat.diffuse;
    float3 Ks = mat.specular;
    float3 H = normalize(V + L);

    float dotHN = clamp(dot(H, N), 0.0f, 1.0f);
    float dotLH = clamp(dot(L, H), 0.0f, 1.0f);
    float dotVH = clamp(dot(V, H), 0.0f, 1.0f);

    // shininess holds GGX roughness; the floor keeps D finite for near-mirror surfaces
    float alpha = max(mat.shininess, 1e-3f);
    float alphaSqr = alpha * alpha;

    // Schlick; the grazing value is scaled so materials authored with Ks = 0 stay non-specular
    float f90 = clamp(50.0f * max(Ks.r, max(Ks.g, Ks.b)), 0.0f, 1.0f);
    float m = 1.0f - dotLH;
    float3 F = Ks + (float3(f90) - Ks) * (m * m * m * m * m);

    float d = dotHN * dotHN * (alphaSqr - 1.0f) + 1.0f;
    float D = alphaSqr / (PI * d * d);

    float G = G1(dotLN, dotLH, alphaSqr) * G1(dotVN, dotVH, alphaSqr);

    float3 specular = (D * F * G) / (4.0f * dotLN * dotVN);
    float3 diffuse = (float3(1.0f) - F) * Kd / PI;  // Energy reflected specularly can't also scatter diffusely

    return dotLN * (diffuse + specular);
}

float3 SampleLobe(float3 A, float c, float phi)
{
    float s = sqrt(max(1.0f - c * c, 0.0f));
    float3 K = float3(s * cos(phi), s * sin(phi), c);

    if (abs(A.z - 1.0) < 1e-3)
        return K;

    if (abs(A.z + 1.0) < 1e-3)
        return float3(K.x, -K.y, -K.z);

    A = normalize(A);
    float3 B = normalize(float3(-A.y, A.x, 0.0));
    float3 C = cross(A, B);

    return K.x * B + K.y * C + K.z * A;
}

// Chance of sampling the specular lobe rather than the diffuse one, from the expected
// reflectance of each. Clamped so that neither lobe is ever left unsampled.
float SpecularProbability(float3 N, float3 V, Material mat)
{
    float f0 = max(mat.specular.r, max(mat.specular.g, mat.specular.b));
    float kd = max(mat.diffuse.r, max(mat.diffuse.g, mat.diffuse.b));

    float m = 1.0f - clamp(dot(N, V), 0.0f, 1.0f);
    float f90 = clamp(50.0f * f0, 0.0f, 1.0f);
    float F = f0 + (f90 - f0) * (m * m * m * m * m);

    float total = F + kd * (1.0f - F);
    return clamp(total > 0.0f ? F / total : 0.5f, 0.1f, 0.9f);
}

// Sample the GGX distribution of normals visible from V, then reflect V (Heitz 2018).
float3 SampleGgx(inout uint seed, float3 N, float3 V, float alpha)
{
    float3 T = abs(N.z) < 0.999f ? normalize(cross(float3(0.0f, 0.0f, 1.0f), N)) : float3(1.0f, 0.0f, 0.0f);
    float3 B = cross(N, T);
    float3 Vlocal = float3(dot(V, T), dot(V, B), dot(V, N));

    float3 Vh = normalize(float3(alpha * Vlocal.x, alpha * Vlocal.y, Vlocal.z));
    float lenSqr = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1 = lenSqr > 0.0f ? float3(-Vh.y, Vh.x, 0.0f) / sqrt(lenSqr) : float3(1.0f, 0.0f, 0.0f);
    float3 T2 = cross(Vh, T1);

    float r = sqrt(rnd(seed));
    float phi = 2.0f * PI * rnd(seed);
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5f * (1.0f + Vh.z);
    t2 = (1.0f - s) * sqrt(max(1.0f - t1 * t1, 0.0f)) + s * t2;

    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(1.0f - t1 * t1 - t2 * t2, 0.0f)) * Vh;
    float3 Hlocal = normalize(float3(alpha * Nh.x, alpha * Nh.y, max(Nh.z, 0.0f)));

    return reflect(-V, Hlocal.x * T + Hlocal.y * B + Hlocal.z * N);
}

float3 SampleBrdf(inout uint seed, float3 N, float3 V, Material mat)
{
    if (rnd(seed) < SpecularProbability(N, V, mat))
        return SampleGgx(seed, N, V, max(mat.shininess, 1e-3f));

    float c = sqrt(rnd(seed));
    float phi = 2.0f * PI * rnd(seed);
    return SampleLobe(N, c, phi);
}

// Pdf of SampleBrdf: both lobes can produce any direction, so this is always their mixture.
float PdfBrdf(float3 N, float3 L, float3 V, Material mat)
{
    float dotLN = dot(L, N);
    float dotVN = dot(V, N);
    if (dotLN <= 0.0f || dotVN <= 0.0f)
        return 0.0f;

    float3 H = normalize(V + L);
    float dotHN = clamp(dot(H, N), 0.0f, 1.0f);
    float dotVH = clamp(dot(V, H), 0.0f, 1.0f);

    float alpha = max(mat.shininess, 1e-3f);
    float alphaSqr = alpha * alpha;
    float d = dotHN * dotHN * (alphaSqr - 1.0f) + 1.0f;
    float D = alphaSqr / (PI * d * d);

    float pdfSpecular = G1(dotVN, dotVH, alphaSqr) * D / (4.0f * dotVN);
    float pdfDiffuse = dotLN / PI;

    float pS = SpecularProbability(N, V, mat);
    return pS * pdfSpecular + (1.0f - pS) * pdfDiffuse;
}

[shader("raygeneration")]
void rayGenMain()
{
    int2 pixel = int2(DispatchRaysIndex().xy);
    uint2 launchSize = DispatchRaysDimensions().xy;

    // Push-constant layout check (see alignmentTest in shared_structs.h)
    if (pcRay.alignmentTest != 1234)
    {
        colCurr.Store(pixel, float4(1, 0, 0, 0));
        return;
    }

    float2 pixelCenter = float2(pixel) + float2(0.5);
    float2 pixelNDC = pixelCenter / float2(launchSize) * 2.0 - 1.0;

    float3 eyeW = mul(mats.viewInverse, float4(0, 0, 0, 1)).xyz;
    float4 pixelH = mul(mats.viewInverse, mul(mats.projInverse, float4(pixelNDC.x, pixelNDC.y, 1, 1)));
    float3 pixelW = pixelH.xyz / pixelH.w;

    RayDesc ray;
    ray.Origin = eyeW;
    ray.Direction = normalize(pixelW - eyeW);
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    RayPayload payload;
    payload.seed = tea(pixel.y * launchSize.x + pixel.x, pcRay.frameSeed);
    float3 C = float3(0.0);
    float3 W = float3(1.0);

    bool firstHit = false;
    float3 firstPos = float3(0.0f);
    float firstDepth = 0.0f;
    float3 firstNrm = float3(0.0f);
    float3 firstKd = float3(0.0f);

    // Paths end by Russian roulette below; pcRay.depth is only an upper limit on their length.
    for (int i = 0; i < pcRay.depth; ++i)
    {
        TraceRay(topLevelAS, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payload);

        if (!payload.hit)
            break;

        ObjDesc objResources = objDesc[payload.instanceIndex];
        Vertex* vertices = (Vertex*)objResources.vertexAddress;
        int3* indices = (int3*)objResources.indexAddress;
        Material* materials = (Material*)objResources.materialAddress;
        int* matIndices = (int*)objResources.materialIndexAddress;

        int3 ind = indices[payload.primitiveIndex];
        Material mat = materials[matIndices[payload.primitiveIndex]];

        Vertex v0 = vertices[ind.x];
        Vertex v1 = vertices[ind.y];
        Vertex v2 = vertices[ind.z];

        float3 bc = payload.bc;
        float3 nrm = bc.x * v0.nrm + bc.y * v1.nrm + bc.z * v2.nrm;
        float2 uv = bc.x * v0.texCoord + bc.y * v1.texCoord + bc.z * v2.texCoord;

        if (mat.textureId >= 0)
        {
            uint txtId = objResources.txtOffset + mat.textureId;
            mat.diffuse = textureSamplers[NonUniformResourceIndex(txtId)].SampleLevel(uv, 0).xyz;
        }

        if (!pcRay.fullBRDF)
            mat.specular = float3(0.0f);

        // Instances have culling disabled, so back faces are hit: orient the normals towards
        // the incoming ray. The shading normal falls back to the geometric one if it faces away.
        float3 Wo = -ray.Direction;

        float3 Ng = cross(v1.pos - v0.pos, v2.pos - v0.pos);
        Ng = dot(Ng, Ng) > 0.0f ? normalize(Ng) : Wo;
        if (dot(Ng, Wo) < 0.0f)
            Ng = -Ng;

        float3 N = dot(nrm, nrm) > 0.0f ? normalize(nrm) : Ng;
        if (dot(N, Ng) < 0.0f)
            N = -N;
        if (dot(N, Wo) <= 0.0f)
            N = Ng;

        if (i == 0)
        {
            firstHit = true;
            firstPos = payload.hitPos;
            firstDepth = mul(mats.viewProj, float4(payload.hitPos, 1.0f)).w;  // View-space depth
            firstNrm = N;
            firstKd = mat.diffuse;
        }

        if (dot(mat.emission, mat.emission) > 0.0f)
        {
            C += mat.emission * pcRay.emissionScale * W;
            break;
        }

        float3 Wi = SampleBrdf(payload.seed, N, Wo, mat);

        // A GGX sample can reflect below N, and a shading normal can send the sample
        // through the surface it was generated from. Neither carries any energy.
        if (dot(Wi, N) <= 0.0f || dot(Wi, Ng) <= 0.0f)
            break;

        float3 f = EvalBrdf(N, Wi, Wo, mat);
        float p = PdfBrdf(N, Wi, Wo, mat);

        if (p < 1e-6)  // Mathematically impossible, but roundoff happens
            break;

        W *= f / p;

        if (rnd(payload.seed) >= pcRay.rr)
            break;
        W /= pcRay.rr;

        ray.Origin = payload.hitPos;
        ray.Direction = Wi;
    }

    float4 P = float4(0.0f);
    float total_weight = 0.0f;

    if (firstHit && pcRay.useHistory && !pcRay.clear)
    {
        // Reproject into the prior frame. For this projection clip w is view-space depth, so
        // screenH.w is the depth this point should have recorded in NdPrev last frame.
        float4 screenH = mul(mats.priorViewProj, float4(firstPos, 1.0f));

        if (screenH.w > 0.0f)
        {
            float2 screen = ((screenH.xy / screenH.w) + float2(1.0f)) / 2.0f;

            if (all(screen >= float2(0.0f)) && all(screen <= float2(1.0f)))
            {
                float2 floc = screen * float2(launchSize) - float2(0.5f);
                float2 base = floor(floc);
                float2 offset = floc - base;
                int2 iloc = int2(base);

                for (int i = 0; i <= 1; ++i)
                {
                    for (int j = 0; j <= 1; ++j)
                    {
                        int2 loc = iloc + int2(i, j);
                        if (any(loc < int2(0)) || any(loc >= int2(launchSize)))
                            continue;

                        float4 prevNd = NdPrev.Load(loc);

                        // d_threshold is the allowed depth difference, as a fraction of the depth
                        if (abs(screenH.w - prevNd.w) >= pcRay.d_threshold * screenH.w)
                            continue;
                        if (dot(firstNrm, prevNd.xyz) <= pcRay.n_threshold)
                            continue;

                        float bilinear_weight = (i == 0 ? 1.0f - offset.x : offset.x)
                                              * (j == 0 ? 1.0f - offset.y : offset.y);

                        total_weight += bilinear_weight;
                        P += colPrev.Load(loc) * bilinear_weight;
                    }
                }
            }
        }
    }

    // With no usable history this starts from a count of 0, so the first sample replaces it.
    float oldN = 0.0f;
    float3 oldAve = float3(0.0f);

    if (total_weight > 1e-6f)
    {
        P /= total_weight;
        oldN = P.w;
        oldAve = P.xyz;

        // Limiting the count keeps new samples weighted at 1/maxSamples or more
        if (pcRay.maxSamples > 0.0f)
            oldN = min(oldN, pcRay.maxSamples - 1.0f);
    }

    // A non-finite sample must not enter the running average, so it is dropped.
    float newN = oldN;
    float3 newAve = oldAve;

    if (!any(isnan(C)) && !any(isinf(C)))
    {
        newN = oldN + 1.0f;
        newAve = oldAve + (C - oldAve) / newN;
    }

    colCurr.Store(pixel, float4(newAve, newN));
    KdCurr.Store(pixel, float4(firstKd, 0.0f));
    NdCurr.Store(pixel, float4(firstNrm, firstDepth));
}

[shader("miss")]
void missMain(inout RayPayload payload)
{
    payload.hit = false;
}

[shader("miss")]
void shadowMissMain(inout RayPayload payload)
{
}

[shader("closesthit")]
void closestHitMain(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    float2 bc = attr.barycentrics;
    payload.hit = true;
    payload.hitDist = RayTCurrent();
    payload.instanceIndex = InstanceID();
    payload.primitiveIndex = PrimitiveIndex();
    payload.bc = float3(1.0 - bc.x - bc.y, bc.x, bc.y);
    payload.hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
}
