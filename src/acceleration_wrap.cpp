#include "acceleration_wrap.h"

#include <assert.h>

#include <algorithm>
#include <vector>

#include "renderer.h"
#include "vk_check.h"

namespace rtrt
{

//----------------------------------------------------------------------------------------
// Initializing the allocator and querying the raytracing properties
//

void RaytracingBuilder::Setup(Renderer* Owner, VkDevice DeviceHandle,
                              uint32_t GraphicsQueueIndex)
{
    VK = Owner;
    Device = DeviceHandle;
    QueueIndex = GraphicsQueueIndex;
}

//----------------------------------------------------------------------------------------
// Destroying all allocations
//
void RaytracingBuilder::Destroy()
{
    for (auto& Blas : BlasList)
    {
        Blas.Bw.Destroy(VK->GetDevice());
        vkDestroyAccelerationStructureKHR(VK->GetDevice(), Blas.Accel, nullptr);
    }

    Tlas.Bw.Destroy(VK->GetDevice());
    vkDestroyAccelerationStructureKHR(VK->GetDevice(), Tlas.Accel, nullptr);

    BlasList.clear();
}

void RaytracingBuilder::DestroyScratchBuffers()
{
    BlasScratch.Destroy(Device);
    TlasScratch.Destroy(Device);
}

//----------------------------------------------------------------------------------------
// Returning the constructed top-level acceleration structure
//
VkAccelerationStructureKHR RaytracingBuilder::GetAccelerationStructure() const
{
    return Tlas.Accel;
}

//----------------------------------------------------------------------------------------
// Return the device address of a Blas previously created.
//
VkDeviceAddress RaytracingBuilder::GetBlasDeviceAddress(uint32_t BlasId)
{
    assert(static_cast<size_t>(BlasId) < BlasList.size());
    VkAccelerationStructureDeviceAddressInfoKHR AddressInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    AddressInfo.accelerationStructure = BlasList[BlasId].Accel;
    return vkGetAccelerationStructureDeviceAddressKHR(Device, &AddressInfo);
}

//----------------------------------------------------------------------------------------
// Create all the BLAS from the vector of BlasInput
// - There will be one BLAS per input-vector entry
// - There will be as many BLAS as input.size()
// - The resulting BLAS (along with the inputs used to build) are stored in BlasList,
//   and can be referenced by index.
// - if flag has the 'Compact' flag, the BLAS will be compacted
//
void RaytracingBuilder::BuildBlas(const std::vector<BlasInput>& Input,
                                  VkBuildAccelerationStructureFlagsKHR Flags)
{
    auto NbBlas = static_cast<uint32_t>(Input.size());
    uint32_t NbCompactions{0};  // Nb of BLAS requesting compaction

    std::vector<BuildAccelerationStructure> BuildAs(NbBlas);
    const VkDeviceSize MaxScratchSize =
        QueryBlasSizes(Input, Flags, BuildAs, NbCompactions);
    const VkDeviceAddress ScratchAddress =
        CreateScratch(BlasScratch, MaxScratchSize, "buildBlas scratch buffer");

    // Allocate a query pool for storing the needed size for every BLAS compaction.
    VkQueryPool QueryPool{VK_NULL_HANDLE};
    if (NbCompactions > 0)  // Is compaction requested?
    {
        assert(NbCompactions == NbBlas);  // Don't allow mix of on/off compaction
        VkQueryPoolCreateInfo Qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        Qpci.queryCount = NbBlas;
        Qpci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
        RTRT_CHECK_VK(vkCreateQueryPool(Device, &Qpci, nullptr, &QueryPool));
    }

    BuildBlasBatches(BuildAs, ScratchAddress, QueryPool);

    // Keeping all the created acceleration structures
    for (auto& Build : BuildAs)
    {
        BlasList.emplace_back(Build.As);
    }

    // Clean up
    vkDestroyQueryPool(Device, QueryPool, nullptr);
}

VkDeviceSize RaytracingBuilder::QueryBlasSizes(
    const std::vector<BlasInput>& Input, VkBuildAccelerationStructureFlagsKHR Flags,
    std::vector<BuildAccelerationStructure>& BuildAs, uint32_t& NbCompactions)
{
    VkDeviceSize MaxScratchSize{0};  // Largest scratch size

    for (uint32_t Idx = 0; Idx < BuildAs.size(); Idx++)
    {
        // Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying
        // the build sizes. The rest is filled in by CmdCreateBlas
        BuildAs[Idx].BuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        BuildAs[Idx].BuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        BuildAs[Idx].BuildInfo.flags = Input[Idx].Flags | Flags;
        BuildAs[Idx].BuildInfo.geometryCount =
            static_cast<uint32_t>(Input[Idx].AsGeometry.size());
        BuildAs[Idx].BuildInfo.pGeometries = Input[Idx].AsGeometry.data();

        // Build range information
        BuildAs[Idx].RangeInfo = Input[Idx].AsBuildOffsetInfo.data();

        // Finding sizes to create acceleration structures and scratch
        std::vector<uint32_t> MaxPrimCount(Input[Idx].AsBuildOffsetInfo.size());
        for (auto Tt = 0; Tt < Input[Idx].AsBuildOffsetInfo.size(); Tt++)
        {
            // # of triangles
            MaxPrimCount[Tt] = Input[Idx].AsBuildOffsetInfo[Tt].primitiveCount;
        }
        vkGetAccelerationStructureBuildSizesKHR(
            Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &BuildAs[Idx].BuildInfo, MaxPrimCount.data(), &BuildAs[Idx].SizeInfo);

        MaxScratchSize = std::max(MaxScratchSize, BuildAs[Idx].SizeInfo.buildScratchSize);
        NbCompactions +=
            HasFlag(BuildAs[Idx].BuildInfo.flags,
                    VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
    }
    return MaxScratchSize;
}

VkDeviceAddress RaytracingBuilder::CreateScratch(BufferWrap& Scratch, VkDeviceSize Size,
                                                 const char* Name)
{
    // Holds the temporary data of the acceleration structure builder
    Scratch = VK->CreateBufferWrap(
        Size,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    SetDebugName(Device, Scratch.Buffer, VK_OBJECT_TYPE_BUFFER, Name);

    VkBufferDeviceAddressInfo BufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                         nullptr, Scratch.Buffer};
    return vkGetBufferDeviceAddress(Device, &BufferInfo);
}

void RaytracingBuilder::BuildBlasBatches(std::vector<BuildAccelerationStructure>& BuildAs,
                                         VkDeviceAddress ScratchAddress,
                                         VkQueryPool QueryPool)
{
    // Batching creation/compaction of BLAS to allow staying in restricted amount of
    // memory
    const auto NbBlas = static_cast<uint32_t>(BuildAs.size());
    std::vector<uint32_t> Indices;  // Indices of the BLAS to create
    VkDeviceSize BatchSize{0};
    VkDeviceSize BatchLimit{256'000'000};  // 256 MB
    for (uint32_t Idx = 0; Idx < NbBlas; Idx++)
    {
        Indices.push_back(Idx);
        BatchSize += BuildAs[Idx].SizeInfo.accelerationStructureSize;
        // Over the limit or last BLAS element
        if (BatchSize >= BatchLimit || Idx == NbBlas - 1)
        {
            VkCommandBuffer CmdBuf = VK->CreateTempCmdBuffer();
            CmdCreateBlas(CmdBuf, Indices, BuildAs, ScratchAddress, QueryPool);
            VK->SubmitTempCmdBuffer(CmdBuf);

            if (QueryPool)
            {
                VkCommandBuffer CmdBuf = VK->CreateTempCmdBuffer();
                CmdCompactBlas(CmdBuf, Indices, BuildAs, QueryPool);
                VK->SubmitTempCmdBuffer(CmdBuf);

                // Destroy the non-compacted version
                DestroyNonCompacted(Indices, BuildAs);
            }

            BatchSize = 0;
            Indices.clear();
        }
    }
}

namespace
{

AccelerationStructureWrap CreateAcceleration(
    Renderer* VK, VkAccelerationStructureCreateInfoKHR& AccelInfo)
{
    AccelerationStructureWrap Result;
    // Allocating the buffer to hold the acceleration structure
    Result.Bw =
        VK->CreateBufferWrap(AccelInfo.size,
                             VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Create the acceleration structure
    AccelInfo.buffer = Result.Bw.Buffer;
    RTRT_CHECK_VK(vkCreateAccelerationStructureKHR(VK->GetDevice(), &AccelInfo, nullptr,
                                                   &Result.Accel));

    return Result;
}

}  // namespace

// Creating the bottom level acceleration structure for all indices of `buildAs` vector.
// The array of BuildAccelerationStructure was created in buildBlas and the vector of
// indices limits the number of BLAS to create at once. This limits the amount of
// memory needed when compacting the BLAS.
void RaytracingBuilder::CmdCreateBlas(VkCommandBuffer CmdBuf,
                                      const std::vector<uint32_t>& Indices,
                                      std::vector<BuildAccelerationStructure>& BuildAs,
                                      VkDeviceAddress ScratchAddress,
                                      VkQueryPool QueryPool)
{
    if (QueryPool)  // For querying the compaction size
    {
        vkResetQueryPool(Device, QueryPool, 0, static_cast<uint32_t>(Indices.size()));
    }
    uint32_t QueryCnt{0};

    for (const auto& Idx : Indices)
    {
        // Actual allocation of buffer and acceleration structure.
        VkAccelerationStructureCreateInfoKHR CreateInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        CreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        // Will be used to allocate memory.
        CreateInfo.size = BuildAs[Idx].SizeInfo.accelerationStructureSize;
        BuildAs[Idx].As = CreateAcceleration(VK, CreateInfo);

        // BuildInfo #2 part
        // Setting where the build lands
        BuildAs[Idx].BuildInfo.dstAccelerationStructure = BuildAs[Idx].As.Accel;
        // All build use the same scratch buffer
        BuildAs[Idx].BuildInfo.scratchData.deviceAddress = ScratchAddress;

        // Building the bottom-level-acceleration-structure
        vkCmdBuildAccelerationStructuresKHR(CmdBuf, 1, &BuildAs[Idx].BuildInfo,
                                            &BuildAs[Idx].RangeInfo);

        // Since the scratch buffer is reused across builds, we
        // need a barrier to ensure one build is finished before
        // starting the next one.
        VkMemoryBarrier Barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        Barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        Barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(CmdBuf,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1,
                             &Barrier, 0, nullptr, 0, nullptr);

        if (QueryPool)
        {
            // Add a query to find the 'real' amount of memory needed, use for compaction
            vkCmdWriteAccelerationStructuresPropertiesKHR(
                CmdBuf, 1, &BuildAs[Idx].BuildInfo.dstAccelerationStructure,
                VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, QueryPool,
                QueryCnt++);
        }
    }
}

//----------------------------------------------------------------------------------------
// Create and replace a new acceleration structure and buffer based on the size retrieved
// by the Query.
void RaytracingBuilder::CmdCompactBlas(VkCommandBuffer CmdBuf,
                                       const std::vector<uint32_t>& Indices,
                                       std::vector<BuildAccelerationStructure>& BuildAs,
                                       VkQueryPool QueryPool)
{
    uint32_t QueryCtn{0};

    // Get the compacted size result back
    std::vector<VkDeviceSize> CompactSizes(static_cast<uint32_t>(Indices.size()));
    RTRT_CHECK_VK(vkGetQueryPoolResults(
        Device, QueryPool, 0, static_cast<uint32_t>(CompactSizes.size()),
        CompactSizes.size() * sizeof(VkDeviceSize), CompactSizes.data(),
        sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT));

    for (auto Idx : Indices)
    {
        BuildAs[Idx].CleanupAs = BuildAs[Idx].As.Accel;  // previous AS to destroy
        // new reduced size
        BuildAs[Idx].SizeInfo.accelerationStructureSize = CompactSizes[QueryCtn++];

        // Creating a compact version of the AS
        VkAccelerationStructureCreateInfoKHR AsCreateInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        AsCreateInfo.size = BuildAs[Idx].SizeInfo.accelerationStructureSize;
        AsCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        BuildAs[Idx].As = CreateAcceleration(VK, AsCreateInfo);

        // Copy the original BLAS to a compact version
        VkCopyAccelerationStructureInfoKHR CopyInfo{
            VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
        CopyInfo.src = BuildAs[Idx].BuildInfo.dstAccelerationStructure;
        CopyInfo.dst = BuildAs[Idx].As.Accel;
        CopyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
        vkCmdCopyAccelerationStructureKHR(CmdBuf, &CopyInfo);
    }
}

//----------------------------------------------------------------------------------------
// Destroy all the non-compacted acceleration structures
//
void RaytracingBuilder::DestroyNonCompacted(
    const std::vector<uint32_t>& Indices,
    std::vector<BuildAccelerationStructure>& BuildAs)
{
    for (auto& I : Indices)
    {
        vkDestroyAccelerationStructureKHR(VK->GetDevice(), BuildAs[I].CleanupAs, nullptr);
    }
}

//----------------------------------------------------------------------------------------
// Low level of Tlas creation
//
void RaytracingBuilder::CmdCreateTlas(VkCommandBuffer CmdBuf, uint32_t CountInstance,
                                      VkDeviceAddress InstBufferAddr,
                                      VkBuildAccelerationStructureFlagsKHR Flags,
                                      bool Update, bool Motion)
{
    // Wraps a device pointer to the above uploaded instances.
    VkAccelerationStructureGeometryInstancesDataKHR InstancesVk{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    InstancesVk.data.deviceAddress = InstBufferAddr;

    // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the
    // instances struct in a union and label it as instance data.
    VkAccelerationStructureGeometryKHR TopAsGeometry{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    TopAsGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    TopAsGeometry.geometry.instances = InstancesVk;

    // Find sizes
    VkAccelerationStructureBuildGeometryInfoKHR BuildInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    BuildInfo.flags = Flags;
    BuildInfo.geometryCount = 1;
    BuildInfo.pGeometries = &TopAsGeometry;
    BuildInfo.mode = Update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
                            : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    BuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    BuildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

    VkAccelerationStructureBuildSizesInfoKHR SizeInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(
        Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &BuildInfo,
        &CountInstance, &SizeInfo);

    // Create TLAS
    if (Update == false)
    {
        VkAccelerationStructureCreateInfoKHR CreateInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        CreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        CreateInfo.size = SizeInfo.accelerationStructureSize;
        Tlas = CreateAcceleration(VK, CreateInfo);
    }

    VkDeviceAddress ScratchAddress = CreateScratch(TlasScratch, SizeInfo.buildScratchSize,
                                                   "cmdCreateTlas scratch buffer");

    // Update build information
    BuildInfo.srcAccelerationStructure = Update ? Tlas.Accel : VK_NULL_HANDLE;
    BuildInfo.dstAccelerationStructure = Tlas.Accel;
    BuildInfo.scratchData.deviceAddress = ScratchAddress;

    // Build Offsets info: n instances
    VkAccelerationStructureBuildRangeInfoKHR BuildOffsetInfo{CountInstance, 0, 0, 0};
    const VkAccelerationStructureBuildRangeInfoKHR* PBuildOffsetInfo = &BuildOffsetInfo;

    // Build the TLAS
    vkCmdBuildAccelerationStructuresKHR(CmdBuf, 1, &BuildInfo, &PBuildOffsetInfo);
}

//----------------------------------------------------------------------------------------
// Refit BLAS number blasIdx from updated buffer contents.
//
void RaytracingBuilder::UpdateBlas(uint32_t BlasIdx, BlasInput& Blas,
                                   VkBuildAccelerationStructureFlagsKHR Flags)
{
    assert(false && "Not used; Not maintained;  Probably leaks a VkDeviceMemory");
    assert(static_cast<size_t>(BlasIdx) < BlasList.size());

    // Preparing all build information, acceleration is filled later
    VkAccelerationStructureBuildGeometryInfoKHR BuildInfos{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    BuildInfos.flags = Flags;
    BuildInfos.geometryCount = static_cast<uint32_t>(Blas.AsGeometry.size());
    BuildInfos.pGeometries = Blas.AsGeometry.data();
    BuildInfos.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;  // UPDATE
    BuildInfos.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    BuildInfos.srcAccelerationStructure = BlasList[BlasIdx].Accel;  // UPDATE
    BuildInfos.dstAccelerationStructure = BlasList[BlasIdx].Accel;

    // Find size to build on the device
    std::vector<uint32_t> MaxPrimCount(Blas.AsBuildOffsetInfo.size());
    for (auto Tt = 0; Tt < Blas.AsBuildOffsetInfo.size(); Tt++)
    {
        // Number of primitives/triangles
        MaxPrimCount[Tt] = Blas.AsBuildOffsetInfo[Tt].primitiveCount;
    }
    VkAccelerationStructureBuildSizesInfoKHR SizeInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(
        Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &BuildInfos,
        MaxPrimCount.data(), &SizeInfo);

    // Allocate the scratch buffer and setting the scratch info
    BufferWrap Scratch = VK->CreateBufferWrap(
        SizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkBufferDeviceAddressInfo BufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    BufferInfo.buffer = Scratch.Buffer;
    BuildInfos.scratchData.deviceAddress = vkGetBufferDeviceAddress(Device, &BufferInfo);

    std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> PBuildOffset(
        Blas.AsBuildOffsetInfo.size());
    for (size_t I = 0; I < Blas.AsBuildOffsetInfo.size(); I++)
    {
        PBuildOffset[I] = &Blas.AsBuildOffsetInfo[I];
    }

    VkCommandBuffer CmdBuf = VK->CreateTempCmdBuffer();

    // Update the instance buffer on the device side and build the TLAS
    // Update the acceleration structure. Note the VK_TRUE parameter to trigger the
    // update, and the existing BLAS being passed and updated in place
    vkCmdBuildAccelerationStructuresKHR(CmdBuf, 1, &BuildInfos, PBuildOffset.data());

    VK->SubmitTempCmdBuffer(CmdBuf);
}

void RaytracingBuilder::BuildTlas(
    const std::vector<VkAccelerationStructureInstanceKHR>& Instances,
    VkBuildAccelerationStructureFlagsKHR Flags, bool Update, bool Motion)
{
    // Cannot call buildTlas twice except to update.
    uint32_t CountInstance = static_cast<uint32_t>(Instances.size());

    // Command buffer to create the TLAS
    VkCommandBuffer CmdBuf = VK->CreateTempCmdBuffer();

    // Create a buffer holding the actual instance data (matrices++) for use by the AS
    // builder
    BufferWrap InstancesBuffer = VK->CreateStagedBufferWrap(
        CmdBuf, Instances,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    VkBufferDeviceAddressInfo BufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                         nullptr, InstancesBuffer.Buffer};
    VkDeviceAddress InstBufferAddr = vkGetBufferDeviceAddress(Device, &BufferInfo);

    // Make sure the copy of the instance buffer are copied before triggering the
    // acceleration structure build
    VkMemoryBarrier Barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    Barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    vkCmdPipelineBarrier(CmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1,
                         &Barrier, 0, nullptr, 0, nullptr);

    // Creating the TLAS
    CmdCreateTlas(CmdBuf, CountInstance, InstBufferAddr, Flags, Update, Motion);

    // Finalizing and destroying temporary data
    VK->SubmitTempCmdBuffer(CmdBuf);

    InstancesBuffer.Destroy(VK->GetDevice());
}

//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
// Convert an OBJ model into the ray tracing geometry used to build the BLAS
//
BlasInput Renderer::ObjectToVkGeometryKhr(const ObjData& Model)
{
    // BLAS builder requires raw device addresses.
    VkBufferDeviceAddressInfo VertexAddressInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, Model.VertexBuffer.Buffer};
    VkDeviceAddress VertexAddress =
        vkGetBufferDeviceAddress(GpuDevice.GetHandle(), &VertexAddressInfo);

    VkBufferDeviceAddressInfo IndexAddressInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, Model.IndexBuffer.Buffer};
    VkDeviceAddress IndexAddress =
        vkGetBufferDeviceAddress(GpuDevice.GetHandle(), &IndexAddressInfo);

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
    AllBlas.reserve(ObjDataList.size());
    for (const auto& Obj : ObjDataList)
    {
        BlasInput Blas = ObjectToVkGeometryKhr(Obj);
        // We could add more geometry in each BLAS, but we add only one for now
        AllBlas.emplace_back(Blas);
    }

    RtBuilder.BuildBlas(AllBlas,
                        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    // TLAS
    std::vector<VkAccelerationStructureInstanceKHR> Tlas;
    Tlas.reserve(ObjInstList.size());
    for (const ObjInst& Inst : ObjInstList)
    {
        VkAccelerationStructureInstanceKHR RayInst{};
        // Position of the instance
        RayInst.transform = ToTransformMatrixKhr(Inst.Transform);
        RayInst.instanceCustomIndex = Inst.ObjIndex;
        RayInst.accelerationStructureReference =
            RtBuilder.GetBlasDeviceAddress(Inst.ObjIndex);
        RayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        RayInst.mask = 0xFF;  //  Only be hit if rayMask & instance.mask != 0
        // Use the same hit group for all objects
        RayInst.instanceShaderBindingTableRecordOffset = 0;
        Tlas.emplace_back(RayInst);
    }

    RtBuilder.BuildTlas(Tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                        false, false);

    RtBuilder.DestroyScratchBuffers();
}

}  // namespace rtrt
