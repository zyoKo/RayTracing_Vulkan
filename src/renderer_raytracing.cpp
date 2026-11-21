#include "renderer.h"

#include <vector>

#include <glm/glm.hpp>

#include "shaders/shared_structs.h"
#include "vk_check.h"

namespace rtrt
{

// The values of everything adjustable in the settings panel
void Renderer::SetDefaultSettings()
{
    PushConstantRay& PcRay = RayTrace.GetConstants();
    // Russian roulette survival chance, tested per bounce in the shader
    PcRay.Rr = 0.7f;
    PcRay.Depth = 16;  // Upper limit on path length; 0.7^16 of paths would reach it
    PcRay.EmissionScale = 1.0f;
    PcRay.FullBrdf = true;
    PcRay.UseHistory = true;
    PcRay.NormalThreshold = 0.80f;
    PcRay.DepthThreshold = 0.04f;
    PcRay.MaxSamples = 0.0f;

    Settings.DenoiseEnabled = true;
    Settings.NumAtrousIterations = 5;
    PushConstantDenoise& PcDenoise = Denoiser.GetConstants();
    PcDenoise.NormFactor = 0.003f;
    PcDenoise.DepthFactor = 0.007f;
    PcDenoise.AlbedoFactor = 0.1f;

    Settings.ViewMode = kViewFinal;
    Settings.Exposure = 1.0f;
    Settings.Gamma = 2.2f;

    ResetRtAccumulation();
}

// Makes the next frame discard all temporal history
void Renderer::ResetRtAccumulation()
{
    ResetHistory = true;
    FrameCount = 0;
}

// Initialize ray tracing
void Renderer::InitRayTracing()
{
    SetDefaultSettings();

    // This initializes the acceleration structure helper class
    SceneAccel.Setup(*GpuDevice, MainCmdPool);
}

//----------------------------------------------------------------------------------------
// This descriptor set holds the Acceleration structure and the output image
//
// Pipeline for the ray tracer: all shaders, raygen, chit, miss
//
// Converts a mesh into the ray tracing geometry used to build its BLAS.
BlasInput Renderer::ObjectToVkGeometryKhr(const Mesh& Model)
{
    // BLAS builder requires raw device addresses.
    VkBufferDeviceAddressInfo VertexAddressInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
        Model.VertexBuffer.GetHandle()};
    VkDeviceAddress VertexAddress =
        vkGetBufferDeviceAddress(GpuDevice->GetHandle(), &VertexAddressInfo);

    VkBufferDeviceAddressInfo IndexAddressInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
        Model.IndexBuffer.GetHandle()};
    VkDeviceAddress IndexAddress =
        vkGetBufferDeviceAddress(GpuDevice->GetHandle(), &IndexAddressInfo);

    uint32_t MaxPrimitiveCount = Model.NbIndices / 3;

    // Describe buffer as array of Vertex.
    VkAccelerationStructureGeometryTrianglesDataKHR Triangles{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    Triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
    Triangles.vertexData.deviceAddress = VertexAddress;
    Triangles.vertexStride = sizeof(Vertex);
    // Describe index data (32-bit unsigned int)
    Triangles.indexType = VK_INDEX_TYPE_UINT32;
    Triangles.indexData.deviceAddress = IndexAddress;
    // Indicate identity transform by setting transformData to null device pointer.
    Triangles.maxVertex = Model.NbVertices;

    // Identify the above data as containing opaque triangles.
    VkAccelerationStructureGeometryKHR AsGeom{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    AsGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    AsGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    AsGeom.geometry.triangles = Triangles;

    // The entire array will be used to build the BLAS.
    VkAccelerationStructureBuildRangeInfoKHR Offset;
    Offset.firstVertex = 0;
    Offset.primitiveCount = MaxPrimitiveCount;
    Offset.primitiveOffset = 0;
    Offset.transformOffset = 0;

    // Our blas is made from only one geometry, but could be made of many geometries
    BlasInput Input;
    Input.AsGeometry.emplace_back(AsGeom);
    Input.AsBuildOffsetInfo.emplace_back(Offset);

    return Input;
}

void Renderer::CreateRtAccelerationStructure()
{
    // BLAS - Storing each primitive in a geometry
    std::vector<BlasInput> AllBlas;
    AllBlas.reserve(MainScene.GetMeshes().size());
    for (const Mesh& Obj : MainScene.GetMeshes())
    {
        BlasInput Blas = ObjectToVkGeometryKhr(Obj);
        // We could add more geometry in each BLAS, but we add only one for now
        AllBlas.emplace_back(Blas);
    }

    SceneAccel.BuildBlas(AllBlas,
                         VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    // TLAS
    std::vector<VkAccelerationStructureInstanceKHR> Tlas;
    Tlas.reserve(MainScene.GetInstances().size());
    for (const MeshInstance& Inst : MainScene.GetInstances())
    {
        VkAccelerationStructureInstanceKHR RayInst{};
        // Position of the instance
        RayInst.transform = ToTransformMatrixKhr(Inst.Transform);
        RayInst.instanceCustomIndex = Inst.ObjIndex;
        RayInst.accelerationStructureReference =
            SceneAccel.GetBlasDeviceAddress(Inst.ObjIndex);
        RayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        RayInst.mask = 0xFF;  //  Only be hit if rayMask & instance.mask != 0
        // Use the same hit group for all objects
        RayInst.instanceShaderBindingTableRecordOffset = 0;
        Tlas.emplace_back(RayInst);
    }

    SceneAccel.BuildTlas(Tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                         false, false);

    SceneAccel.DestroyScratchBuffers();
}

}  // namespace rtrt
