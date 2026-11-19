#include "shared_structs.h"

[[vk::binding(eMatrices, 0)]] ConstantBuffer<MatrixUniforms> mats;
[[vk::binding(eObjDescs, 0)]] StructuredBuffer<ObjDesc> objDesc;
[[vk::binding(eTextures, 0)]] Sampler2D textureSamplers[];

[[vk::push_constant]] ConstantBuffer<PushConstantRaster> pcRaster;

static const float PI = 3.14159;

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal   : NORMAL;
    [[vk::location(2)]] float2 texCoord : TEXCOORD;
};

struct Varyings
{
    float4 position : SV_Position;
    [[vk::location(1)]] float3 worldPos : WORLDPOS;
    [[vk::location(2)]] float3 worldNrm : WORLDNRM;
    [[vk::location(3)]] float3 viewDir  : VIEWDIR;
    [[vk::location(4)]] float2 texCoord : TEXCOORD;
};

[shader("vertex")]
Varyings vertexMain(VertexInput input)
{
    float3 eye = mul(mats.viewInverse, float4(0, 0, 0, 1)).xyz;

    Varyings output;
    output.worldPos = mul(pcRaster.modelMatrix, float4(input.position, 1.0)).xyz;
    output.viewDir  = eye - output.worldPos;
    output.texCoord = input.texCoord;
    output.worldNrm = mul((float3x3)pcRaster.modelMatrix, input.normal);
    output.position = mul(mats.viewProj, float4(output.worldPos, 1.0));
    return output;
}

[shader("fragment")]
float4 fragmentMain(Varyings input, uint primitiveId : SV_PrimitiveID) : SV_Target0
{
    ObjDesc obj = objDesc[pcRaster.objIndex];
    int* matIndices = (int*)obj.materialIndexAddress;
    Material* materials = (Material*)obj.materialAddress;
    Material mat = materials[matIndices[primitiveId]];

    float3 N = normalize(input.worldNrm);
    float3 L = normalize(pcRaster.lightPosition - input.worldPos);
    float NL = max(dot(N, L), 0.0);

    float3 Kd = mat.diffuse;
    if (mat.textureId >= 0)
    {
        uint txtId = obj.txtOffset + mat.textureId;
        Kd = textureSamplers[NonUniformResourceIndex(txtId)].Sample(input.texCoord).xyz;
    }

    return float4(pcRaster.lightIntensity * NL * Kd / PI, 1.0);
}
