#ifndef RTRT_SRC_SCENE_SCENE_H_
#define RTRT_SRC_SCENE_SCENE_H_

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "scene/mesh.h"
#include "shaders/shared_structs.h"
#include "vk/buffer.h"
#include "vk/command_pool.h"
#include "vk/device.h"
#include "vk/image.h"

namespace rtrt
{

// The meshes, instances, textures and emitters loaded from model files, and the device
// buffer of object descriptions the shaders index.
class Scene
{
public:
    Scene() = default;
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Reads FileName with Assimp, uploads its geometry, materials and textures, and adds
    // one instance of it placed by Transform.
    void LoadModel(const Device& Gpu, const CommandPool& Pool,
                   const std::string& FileName, const glm::mat4& Transform);

    // Uploads the object descriptions. Call after the last LoadModel.
    void CreateDescriptionBuffer(const Device& Gpu, const CommandPool& Pool);

    void Destroy();

    const std::vector<Mesh>& GetMeshes() const
    {
        return Meshes;
    }

    const std::vector<MeshInstance>& GetInstances() const
    {
        return Instances;
    }

    const std::vector<Image>& GetTextures() const
    {
        return Textures;
    }

    const Buffer& GetDescriptionBuffer() const
    {
        return DescriptionBuffer;
    }

private:
    std::vector<Mesh> Meshes;
    // Device addresses of each mesh's buffers, in mesh order
    std::vector<ObjDesc> Descriptions;
    std::vector<Image> Textures;
    std::vector<MeshInstance> Instances;
    // Emissive triangles, not uploaded yet
    std::vector<Emitter> Emitters;
    Buffer DescriptionBuffer;
};

}  // namespace rtrt

#endif  // RTRT_SRC_SCENE_SCENE_H_
