#ifndef RTRT_SRC_SCENE_MESH_H_
#define RTRT_SRC_SCENE_MESH_H_

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "shaders/shared_structs.h"
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

    std::string Name;
    // Host copies of what MatColorBuffer holds, in the same order
    std::vector<Material> Materials;
    std::vector<std::string> MaterialNames;
    std::vector<uint32_t> MaterialTriangles;
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
