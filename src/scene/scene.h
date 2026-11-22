#ifndef RTRT_SRC_SCENE_SCENE_H_
#define RTRT_SRC_SCENE_SCENE_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

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

    // File names of GetTextures(), in the same order.
    const std::vector<std::string>& GetTextureNames() const
    {
        return TextureNames;
    }

    // Changes a material on the host; RecordMaterialUpdates copies it to the device.
    void SetMaterial(uint32_t MeshIndex, uint32_t MaterialIndex, const Material& Value);

    // Records the copies of every material changed since the last call, with barriers
    // against the shaders that read them. Returns true if anything changed.
    bool RecordMaterialUpdates(VkCommandBuffer Cmd);

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
    std::vector<std::string> TextureNames;
    // Mesh and material index pairs waiting for RecordMaterialUpdates
    std::vector<std::pair<uint32_t, uint32_t>> DirtyMaterials;
    std::vector<MeshInstance> Instances;
    // Emissive triangles, not uploaded yet
    std::vector<Emitter> Emitters;
    Buffer DescriptionBuffer;
};

}  // namespace rtrt

#endif  // RTRT_SRC_SCENE_SCENE_H_
