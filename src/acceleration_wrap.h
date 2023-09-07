
#pragma once

#include <mutex>
#include <vector>
#include <vulkan/vulkan_core.h>

#if VK_KHR_acceleration_structure

#include <type_traits>
#include <string.h>

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "buffer_wrap.h"
//#include "image_wrap.h"

class VkApp;

// Convert a Mat4x4 to the matrix required by acceleration structures
inline VkTransformMatrixKHR toTransformMatrixKHR(glm::mat4 matrix)
{
    // VkTransformMatrixKHR uses a row-major layout, while glm::mat4
    // uses a column-major layout. So we transpose the matrix to
    // memcpy its data directly.
    glm::mat4        temp = glm::transpose(matrix);
    VkTransformMatrixKHR out_matrix;
    memcpy(&out_matrix, &temp, sizeof(VkTransformMatrixKHR));
    return out_matrix;
}

struct WrapAccelerationStructure
{
    VkAccelerationStructureKHR accel;
    BufferWrap bw;
};


// Inputs used to build Bottom-level acceleration structure.
// You manage the lifetime of the buffer(s) referenced by the VkAccelerationStructureGeometryKHRs within.
// In particular, you must make sure they are still valid and not being modified when the BLAS is built or updated.
struct BlasInput
{
    // Data used to build acceleration structure geometry
    std::vector<VkAccelerationStructureGeometryKHR>       asGeometry;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
    VkBuildAccelerationStructureFlagsKHR                  flags{0};
};


// Ray tracing BLAS and TLAS builder
class RaytracingBuilderKHR
{
public:
    VkApp* VK;
    // Initializing the allocator and querying the raytracing properties
    void setup(VkApp* _VK, const VkDevice& device, uint32_t queueIndex);

    // Destroying all allocations
    void destroy();

    // Returning the constructed top-level acceleration structure
    VkAccelerationStructureKHR getAccelerationStructure() const;

    // Return the Acceleration Structure Device Address of a BLAS Id
    VkDeviceAddress getBlasDeviceAddress(uint32_t blasId);

    // Create all the BLAS from the vector of BlasInput
    void buildBlas(const std::vector<BlasInput>&        input,
                   VkBuildAccelerationStructureFlagsKHR flags
                       = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    // Refit BLAS number blasIdx from updated buffer contents.
    void updateBlas(uint32_t blasIdx, BlasInput& blas, VkBuildAccelerationStructureFlagsKHR flags);

    // Build TLAS from an array of VkAccelerationStructureInstanceKHR
    // - Use motion=true with VkAccelerationStructureMotionInstanceNV
    // - The resulting TLAS will be stored in m_tlas
    // - update is to rebuild the Tlas with updated matrices, flag must have the 'allow_update'

    void buildTlas(const std::vector<VkAccelerationStructureInstanceKHR>& instances,
                   VkBuildAccelerationStructureFlagsKHR flags
                       = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                   bool                                 update = false,
                   bool                                 motion = false);

    // Creating the TLAS, called by buildTlas
    void cmdCreateTlas(VkCommandBuffer                      cmdBuf,          // Command buffer
                       uint32_t                             countInstance,   // number of instances
                       VkDeviceAddress                      instBufferAddr,  // Buffer address of instances
                       VkBuildAccelerationStructureFlagsKHR flags,           // Build creation flag
                       bool                                 update,          // Update == animation
                       bool                                 motion           // Motion Blur
                       );


protected:
    std::vector<WrapAccelerationStructure> m_blas;  // Bottom-level acceleration structure
    WrapAccelerationStructure              m_tlas;  // Top-level acceleration structure
    
    // Setup
    VkDevice                 m_device{VK_NULL_HANDLE};
    uint32_t                 m_queueIndex{0};

    struct BuildAccelerationStructure
    {
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo
            {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo
            {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;
        WrapAccelerationStructure as;  // result acceleration structure
        VkAccelerationStructureKHR cleanupAS;
    };


    void cmdCreateBlas(VkCommandBuffer                          cmdBuf,
                       std::vector<uint32_t>                    indices,
                       std::vector<BuildAccelerationStructure>& buildAs,
                       VkDeviceAddress                          scratchAddress,
                       VkQueryPool                              queryPool);
    void cmdCompactBlas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
                        std::vector<BuildAccelerationStructure>& buildAs, VkQueryPool queryPool);
    void destroyNonCompacted(std::vector<uint32_t> indices,
                             std::vector<BuildAccelerationStructure>& buildAs);
    bool hasFlag(VkFlags item, VkFlags flag) { return (item & flag) == flag; }
};


#else
#error This include requires VK_KHR_acceleration_structure support in the Vulkan SDK.
#endif
