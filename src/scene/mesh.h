#ifndef RTRT_SRC_SCENE_MESH_H_
#define RTRT_SRC_SCENE_MESH_H_

#include <cstdint>

#include <glm/glm.hpp>

#include "vk/buffer.h"

namespace rtrt
{

// One model's geometry and materials in device buffers.
struct Mesh
{
    uint32_t NbIndices{0};
    uint32_t NbVertices{0};
    glm::mat4 Transform{1.0f};
    Buffer VertexBuffer;
    // Triangle indices
    Buffer IndexBuffer;
    Buffer MatColorBuffer;
    // Each triangle's material index
    Buffer MatIndexBuffer;
};

// A placement of one mesh in the world.
struct MeshInstance
{
    glm::mat4 Transform{1.0f};
    // Index into Scene::GetMeshes()
    uint32_t ObjIndex{0};
};

}  // namespace rtrt

#endif  // RTRT_SRC_SCENE_MESH_H_
