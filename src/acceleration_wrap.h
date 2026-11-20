#ifndef RTRT_SRC_ACCELERATION_WRAP_H_
#define RTRT_SRC_ACCELERATION_WRAP_H_

#include <string.h>

#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "buffer_wrap.h"

#if VK_KHR_acceleration_structure

namespace rtrt
{

class Renderer;

// Convert a Mat4x4 to the matrix required by acceleration structures
inline VkTransformMatrixKHR ToTransformMatrixKhr(const glm::mat4& Matrix)
{
    // VkTransformMatrixKHR uses a row-major layout, while glm::mat4
    // uses a column-major layout. So we transpose the matrix to
    // memcpy its data directly.
    glm::mat4 Temp = glm::transpose(Matrix);
    VkTransformMatrixKHR OutMatrix;
    memcpy(&OutMatrix, &Temp, sizeof(VkTransformMatrixKHR));
    return OutMatrix;
}

struct AccelerationStructureWrap
{
    VkAccelerationStructureKHR Accel;
    BufferWrap Bw;
};

// Inputs used to build Bottom-level acceleration structure.
// You manage the lifetime of the buffer(s) referenced by the
// VkAccelerationStructureGeometryKHRs within. In particular, you must make sure they are
// still valid and not being modified when the BLAS is built or updated.
struct BlasInput
{
    // Data used to build acceleration structure geometry
    std::vector<VkAccelerationStructureGeometryKHR> AsGeometry;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> AsBuildOffsetInfo;
    VkBuildAccelerationStructureFlagsKHR Flags{0};
};

// Ray tracing BLAS and TLAS builder
class RaytracingBuilder
{
public:
    RaytracingBuilder() = default;
    RaytracingBuilder(const RaytracingBuilder&) = delete;
    RaytracingBuilder& operator=(const RaytracingBuilder&) = delete;

    Renderer* VK;
    // Initializing the allocator and querying the raytracing properties
    void Setup(Renderer* Owner, VkDevice DeviceHandle, uint32_t GraphicsQueueIndex);

    // Destroying all allocations
    void Destroy();

    // Returning the constructed top-level acceleration structure
    VkAccelerationStructureKHR GetAccelerationStructure() const;

    // Return the Acceleration Structure Device Address of a BLAS Id
    VkDeviceAddress GetBlasDeviceAddress(uint32_t BlasId);

    // Create all the BLAS from the vector of BlasInput
    void BuildBlas(const std::vector<BlasInput>& Input,
                   VkBuildAccelerationStructureFlagsKHR Flags =
                       VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    // Refit BLAS number blasIdx from updated buffer contents.
    void UpdateBlas(uint32_t BlasIdx, BlasInput& Blas,
                    VkBuildAccelerationStructureFlagsKHR Flags);

    // Build TLAS from an array of VkAccelerationStructureInstanceKHR
    // - Use motion=true with VkAccelerationStructureMotionInstanceNV
    // - The resulting TLAS will be stored in Tlas
    // - update is to rebuild the Tlas with updated matrices, flag must have the
    // 'allow_update'

    void BuildTlas(const std::vector<VkAccelerationStructureInstanceKHR>& Instances,
                   VkBuildAccelerationStructureFlagsKHR Flags =
                       VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                   bool Update = false, bool Motion = false);

    // Creating the TLAS, called by buildTlas
    void CmdCreateTlas(VkCommandBuffer CmdBuf,          // Command buffer
                       uint32_t CountInstance,          // number of instances
                       VkDeviceAddress InstBufferAddr,  // Buffer address of instances
                       VkBuildAccelerationStructureFlagsKHR Flags,  // Build creation flag
                       bool Update,                                 // Update == animation
                       bool Motion                                  // Motion Blur
    );

protected:
    // Bottom-level acceleration structure
    std::vector<AccelerationStructureWrap> BlasList;
    AccelerationStructureWrap Tlas;  // Top-level acceleration structure

    // Setup
    VkDevice Device{VK_NULL_HANDLE};
    uint32_t QueueIndex{0};

    struct BuildAccelerationStructure
    {
        VkAccelerationStructureBuildGeometryInfoKHR BuildInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        VkAccelerationStructureBuildSizesInfoKHR SizeInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        const VkAccelerationStructureBuildRangeInfoKHR* RangeInfo;
        AccelerationStructureWrap As;  // result acceleration structure
        VkAccelerationStructureKHR CleanupAs;
    };

    void CmdCreateBlas(VkCommandBuffer CmdBuf, const std::vector<uint32_t>& Indices,
                       std::vector<BuildAccelerationStructure>& BuildAs,
                       VkDeviceAddress ScratchAddress, VkQueryPool QueryPool);
    void CmdCompactBlas(VkCommandBuffer CmdBuf, const std::vector<uint32_t>& Indices,
                        std::vector<BuildAccelerationStructure>& BuildAs,
                        VkQueryPool QueryPool);
    void DestroyNonCompacted(const std::vector<uint32_t>& Indices,
                             std::vector<BuildAccelerationStructure>& BuildAs);
    bool HasFlag(VkFlags Item, VkFlags Flag)
    {
        return (Item & Flag) == Flag;
    }
};

}  // namespace rtrt

#else
#error This include requires VK_KHR_acceleration_structure support in the Vulkan SDK.
#endif

#endif  // RTRT_SRC_ACCELERATION_WRAP_H_
