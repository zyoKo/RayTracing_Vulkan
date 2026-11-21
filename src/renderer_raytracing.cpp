#include "renderer.h"

#include <vector>

#include <glm/glm.hpp>

#include "shaders/shared_structs.h"
#include "vk_check.h"

namespace rtrt
{

void Renderer::CreateRtBuffers()
{
    // Note: This will grow to create more than the single buffer RtColCurrBuffer.
    RtColCurrBuffer = CreateBufferImage(MainSwapchain->GetExtent());
    RtColCurrBuffer.TransitionLayout(MainCmdPool, VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_GENERAL);

    RtColPrevBuffer = CreateBufferImage(MainSwapchain->GetExtent());
    RtColPrevBuffer.TransitionLayout(MainCmdPool, VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_GENERAL);

    RtNdCurrBuffer = CreateBufferImage(MainSwapchain->GetExtent());
    RtNdCurrBuffer.TransitionLayout(MainCmdPool, VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_GENERAL);

    RtNdPrevBuffer = CreateBufferImage(MainSwapchain->GetExtent());
    RtNdPrevBuffer.TransitionLayout(MainCmdPool, VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_GENERAL);

    RtKdCurrBuffer = CreateBufferImage(MainSwapchain->GetExtent());
    RtKdCurrBuffer.TransitionLayout(MainCmdPool, VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_GENERAL);

    RtKdPrevBuffer = CreateBufferImage(MainSwapchain->GetExtent());
    RtKdPrevBuffer.TransitionLayout(MainCmdPool, VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_GENERAL);
}

// The values of everything adjustable in the settings panel
void Renderer::SetDefaultSettings()
{
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
void Renderer::CreateRtDescriptorSet()
{
    // TLAS, then the output image and the six history images
    DescriptorBuilder()
        .AddAccelerationStructure(VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                  VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .AddStorageImage(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddStorageImage(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddStorageImage(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddStorageImage(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddStorageImage(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddStorageImage(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .Build(GpuDevice->GetHandle(), RtDesc);

    RtDesc.Set.Write(0, SceneAccel.GetAccelerationStructure());
    RtDesc.Set.Write(1, RtColCurrBuffer.Descriptor());
    RtDesc.Set.Write(2, RtColPrevBuffer.Descriptor());
    RtDesc.Set.Write(3, RtNdCurrBuffer.Descriptor());
    RtDesc.Set.Write(4, RtNdPrevBuffer.Descriptor());
    RtDesc.Set.Write(5, RtKdCurrBuffer.Descriptor());
    RtDesc.Set.Write(6, RtKdPrevBuffer.Descriptor());
}

// Pipeline for the ray tracer: all shaders, raygen, chit, miss
//
void Renderer::CreateRtPipeline()
{
    std::vector<VkPipelineShaderStageCreateInfo> Stages{};
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> Groups{};
    // The pipeline keeps compiled copies, so the modules are freed when this returns.
    std::vector<Shader> Modules;
    CollectRtShaders(Modules, Stages, Groups);

    CreateRtPipelineLayout();

    // Assemble the shader stages and recursion depth info into the ray tracing pipeline
    VkRayTracingPipelineCreateInfoKHR RayPipelineInfo{
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    // Stages are shaders
    RayPipelineInfo.stageCount = static_cast<uint32_t>(Stages.size());
    RayPipelineInfo.pStages = Stages.data();

    RayPipelineInfo.groupCount = static_cast<uint32_t>(Groups.size());
    RayPipelineInfo.pGroups = Groups.data();

    RayPipelineInfo.maxPipelineRayRecursionDepth = 10;  // Ray depth
    RayPipelineInfo.layout = RtPipelineLayout.GetHandle();

    RtPipeline.CreateRayTracing(GpuDevice->GetHandle(), RayPipelineInfo);
}

void Renderer::CollectRtShaders(std::vector<Shader>& Modules,
                                std::vector<VkPipelineShaderStageCreateInfo>& Stages,
                                std::vector<VkRayTracingShaderGroupCreateInfoKHR>& Groups)
{
    // Raygen and miss shaders get their own groups. Hit shaders can group with any-hit
    // and intersection shaders -- of which we have none -- so the hit shader gets its
    // own group too.
    VkPipelineShaderStageCreateInfo Stage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    Stage.pName = "main";  // All the same entry point

    VkRayTracingShaderGroupCreateInfoKHR Group{
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
    Group.anyHitShader = VK_SHADER_UNUSED_KHR;
    Group.closestHitShader = VK_SHADER_UNUSED_KHR;
    Group.generalShader = VK_SHADER_UNUSED_KHR;
    Group.intersectionShader = VK_SHADER_UNUSED_KHR;

    // Raygen shader stage and group appended to stages and groups lists
    Modules.emplace_back().Load(GpuDevice->GetHandle(), "spv/raytrace.rgen.spv",
                                "spv/raytrace.rgen.slang.spv", UseSlang);
    Stage.module = Modules.back().GetHandle();
    Stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    Stages.push_back(Stage);

    Group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    // Index of raygen shader
    Group.generalShader = static_cast<uint32_t>(Stages.size() - 1);
    Groups.push_back(Group);
    Group.generalShader = VK_SHADER_UNUSED_KHR;

    // Miss shader stage and group appended to stages and groups lists
    Modules.emplace_back().Load(GpuDevice->GetHandle(), "spv/raytrace.rmiss.spv",
                                "spv/raytrace.rmiss.slang.spv", UseSlang);
    Stage.module = Modules.back().GetHandle();
    Stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    Stages.push_back(Stage);

    Group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    // Index of miss shader
    Group.generalShader = static_cast<uint32_t>(Stages.size() - 1);
    Groups.push_back(Group);
    Group.generalShader = VK_SHADER_UNUSED_KHR;

    // Closest hit shader stage and group appended to stages and groups lists
    Modules.emplace_back().Load(GpuDevice->GetHandle(), "spv/raytrace.rchit.spv",
                                "spv/raytrace.rchit.slang.spv", UseSlang);
    Stage.module = Modules.back().GetHandle();
    Stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    Stages.push_back(Stage);

    Group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    // Index of hit shader
    Group.closestHitShader = static_cast<uint32_t>(Stages.size() - 1);
    Groups.push_back(Group);
}

void Renderer::CreateRtPipelineLayout()
{
    // Push constant: we want to be able to update constants used by the shaders
    VkPushConstantRange PushConstant{VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                         VK_SHADER_STAGE_MISS_BIT_KHR,
                                     0, sizeof(PushConstantRay)};

    // Descriptor sets: one specific to ray tracing, and one shared with the rasterization
    // pipeline
    RtPipelineLayout.Create(GpuDevice->GetHandle(),
                            {RtDesc.Layout.GetHandle(), ScDesc.Layout.GetHandle()},
                            {PushConstant});
}

void Renderer::Raytrace()
{
    // The push constants for the ray tracing pipeline.
    // These are temporary -- used only for ray casting step.
    PcRay.AlignmentTest = 1234;
    PcRay.TempLightPos = vec4(0.5f, 2.5f, 3.0f, 0.0);
    PcRay.TempLightInt = vec4(2.5, 2.5, 2.5, 0.0);
    PcRay.TempAmbient = vec4(0.2f);

    PcRay.FrameSeed = rand() % 32768;
    PcRay.Clear = ResetHistory;
    ResetHistory = false;

    // Bind the ray tracing pipeline
    vkCmdBindPipeline(FrameCmd.GetHandle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                      RtPipeline.GetHandle());

    // Bind the descriptor sets (the ray tracing specific one, and the
    // full model descriptor)
    std::vector<VkDescriptorSet> DescSets{RtDesc.Set.GetHandle(), ScDesc.Set.GetHandle()};
    vkCmdBindDescriptorSets(FrameCmd.GetHandle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            RtPipelineLayout.GetHandle(), 0,
                            static_cast<uint32_t>(DescSets.size()), DescSets.data(), 0,
                            nullptr);

    // Push the push constants
    vkCmdPushConstants(FrameCmd.GetHandle(), RtPipelineLayout.GetHandle(),
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR,
                       0, sizeof(PushConstantRay), &PcRay);

    // This dispatches the ray generation shader for each pixel on screen.
    RtSbt.CmdTraceRays(FrameCmd.GetHandle(), MainSwapchain->GetExtent().width,
                       MainSwapchain->GetExtent().height);
    FrameCount++;

    // Copy the ray tracer output image to the scanline output image
    // -- because we already have the operations needed to display
    // that image on the screen.  The debug views show a G-buffer image instead.
    if (Settings.ViewMode == kViewAlbedo)
    {
        CmdCopyImage(FrameCmd.GetHandle(), RtKdCurrBuffer, ScImageBuffer,
                     MainSwapchain->GetExtent());
    }
    else if (Settings.ViewMode == kViewNormal || Settings.ViewMode == kViewDepth)
    {
        CmdCopyImage(FrameCmd.GetHandle(), RtNdCurrBuffer, ScImageBuffer,
                     MainSwapchain->GetExtent());
    }
    else
    {
        CmdCopyImage(FrameCmd.GetHandle(), RtColCurrBuffer, ScImageBuffer,
                     MainSwapchain->GetExtent());
    }

    // Save current frame buffers as history for the next frame
    CmdCopyImage(FrameCmd.GetHandle(), RtColCurrBuffer, RtColPrevBuffer,
                 MainSwapchain->GetExtent());
    CmdCopyImage(FrameCmd.GetHandle(), RtKdCurrBuffer, RtKdPrevBuffer,
                 MainSwapchain->GetExtent());
    CmdCopyImage(FrameCmd.GetHandle(), RtNdCurrBuffer, RtNdPrevBuffer,
                 MainSwapchain->GetExtent());
}

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
