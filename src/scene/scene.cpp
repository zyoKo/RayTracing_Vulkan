//////////////////////////////////////////////////////////////////////
// Uses the ASSIMP library to read mesh models in of 30+ file types
// into a structure suitable for the raytracer.
////////////////////////////////////////////////////////////////////////

#include "scene/scene.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>
#include <glm/glm.hpp>

#include "scene/texture.h"
#include "shaders/shared_structs.h"
#include "vk_check.h"

namespace fs = std::filesystem;

namespace rtrt
{

namespace
{

// Local objects and procedures defined and used here:
struct ModelData
{
    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indicies;
    std::vector<Material> Materials;
    std::vector<int32_t> MatIndx;
    std::vector<std::string> Textures;
    std::vector<std::string> MaterialNames;

    void ReadAssimpFile(const std::string& Path, const mat4& M);
    // Appends one Assimp material, and its diffuse texture if it has one.
    void ReadMaterial(const aiMaterial* Mtl, const std::string& Path);
};

// Reads the file with Assimp; exits with an error if it's missing or unreadable.
const aiScene* ImportScene(Assimp::Importer& Importer, const std::string& Path);

void RecurseModelNodes(ModelData* Meshdata, const aiScene* Aiscene, const aiNode* Node,
                       const aiMatrix4x4& ParentTr, const int Level = 0);

}  // namespace

namespace
{

void PrintModelSummary(const ModelData& Meshdata)
{
    printf("vertices: %zd\n", Meshdata.Vertices.size());
    printf("indices: %zd (%zd)\n", Meshdata.Indicies.size(),
           Meshdata.Indicies.size() / 3);
    printf("materials: %zd\n", Meshdata.Materials.size());
    printf("matIndx: %zd\n", Meshdata.MatIndx.size());
    printf("textures: %zd\n", Meshdata.Textures.size());
}

// Appends the model's emissive geometry as light-emitting triangles.
void CollectEmitters(const ModelData& Meshdata, std::vector<Emitter>& LightList)
{
    for (int I = 0; I < Meshdata.Materials.size(); ++I)
    {
        if (Meshdata.Materials[I].Emission != vec3(0.0f))
        {
            Emitter Light;
            Light.V0 = Meshdata.Vertices[Meshdata.Indicies[I * 3]].Pos;
            Light.V1 = Meshdata.Vertices[Meshdata.Indicies[3 * I + 1]].Pos;
            Light.V2 = Meshdata.Vertices[Meshdata.Indicies[3 * I + 2]].Pos;
            Light.Emission = 4.0f * Meshdata.Materials[I].Emission;
            Light.Index = I;

            LightList.push_back(Light);
        }
    }
}

// Creates the buffers on the device and copies vertices, indices and materials in.
Mesh UploadModel(const Device& Gpu, const CommandPool& Pool, const ModelData& Meshdata)
{
    Mesh Object;
    Object.NbIndices = static_cast<uint32_t>(Meshdata.Indicies.size());
    Object.NbVertices = static_cast<uint32_t>(Meshdata.Vertices.size());

    VkBufferUsageFlags Flag =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VkBufferUsageFlags RtFlags =
        Flag | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

    Object.VertexBuffer.CreateWithData(Gpu, Pool, Meshdata.Vertices,
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | RtFlags);
    Object.IndexBuffer.CreateWithData(Gpu, Pool, Meshdata.Indicies,
                                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | RtFlags);
    Object.MatColorBuffer.CreateWithData(Gpu, Pool, Meshdata.Materials, Flag);
    Object.MatIndexBuffer.CreateWithData(Gpu, Pool, Meshdata.MatIndx, Flag);
    return Object;
}

}  // namespace

void Scene::LoadModel(const Device& Gpu, const CommandPool& Pool,
                      const std::string& FileName, const glm::mat4& Transform)
{
    ModelData Meshdata;
    Meshdata.ReadAssimpFile(FileName, glm::mat4(1.0f));
    PrintModelSummary(Meshdata);

    // The model's emitters are too dim as authored; boost them.
    for (auto& Mat : Meshdata.Materials)
    {
        if (Mat.Emission != vec3(0))
        {
            Mat.Emission *= 2.5f;
        }
    }

    CollectEmitters(Meshdata, Emitters);

    Mesh Object = UploadModel(Gpu, Pool, Meshdata);
    Object.Name = fs::path(FileName).stem().string();
    Object.Materials = Meshdata.Materials;
    Object.MaterialNames = Meshdata.MaterialNames;
    Object.MaterialTriangles.assign(Meshdata.Materials.size(), 0);
    for (const int32_t Index : Meshdata.MatIndx)
    {
        if (Index >= 0 && static_cast<size_t>(Index) < Object.MaterialTriangles.size())
        {
            Object.MaterialTriangles[Index]++;
        }
    }

    // Creates all textures on the GPU
    // Offset is current size
    const auto TxtOffset = static_cast<uint32_t>(Textures.size());
    for (const auto& TexName : Meshdata.Textures)
    {
        Textures.push_back(LoadTexture(Gpu, Pool, TexName));
        TextureNames.push_back(fs::path(TexName).filename().string());
    }

    // Assuming one instance of an object with its supplied transform.
    // Could provide multiple transform here to make a vector of instances of this object.
    MeshInstance Instance;
    Instance.Transform = Transform;
    Instance.ObjIndex = static_cast<uint32_t>(Meshes.size());
    Instances.push_back(Instance);

    // Creating information for device access
    ObjDesc Desc;
    Desc.TxtOffset = TxtOffset;
    Desc.VertexAddress = Object.VertexBuffer.GetAddress();
    Desc.IndexAddress = Object.IndexBuffer.GetAddress();
    Desc.MaterialAddress = Object.MatColorBuffer.GetAddress();
    Desc.MaterialIndexAddress = Object.MatIndexBuffer.GetAddress();

    Meshes.emplace_back(std::move(Object));
    Descriptions.emplace_back(Desc);
}

void Scene::CreateDescriptionBuffer(const Device& Gpu, const CommandPool& Pool)
{
    DescriptionBuffer.CreateWithData(Gpu, Pool, Descriptions,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

void Scene::SetMaterial(uint32_t MeshIndex, uint32_t MaterialIndex, const Material& Value)
{
    Meshes[MeshIndex].Materials[MaterialIndex] = Value;
    const std::pair<uint32_t, uint32_t> Key{MeshIndex, MaterialIndex};
    if (std::find(DirtyMaterials.begin(), DirtyMaterials.end(), Key) ==
        DirtyMaterials.end())
    {
        DirtyMaterials.push_back(Key);
    }
}

bool Scene::RecordMaterialUpdates(VkCommandBuffer Cmd)
{
    if (DirtyMaterials.empty())
    {
        return false;
    }

    const VkPipelineStageFlags ReaderStages =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    VkMemoryBarrier Barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    Barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    Barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(Cmd, ReaderStages, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1,
                         &Barrier, 0, nullptr, 0, nullptr);

    for (const auto& [MeshIndex, MaterialIndex] : DirtyMaterials)
    {
        const Mesh& Object = Meshes[MeshIndex];
        vkCmdUpdateBuffer(Cmd, Object.MatColorBuffer.GetHandle(),
                          sizeof(Material) * MaterialIndex, sizeof(Material),
                          &Object.Materials[MaterialIndex]);
    }
    DirtyMaterials.clear();

    Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(Cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, ReaderStages, 0, 1,
                         &Barrier, 0, nullptr, 0, nullptr);
    return true;
}

void Scene::Destroy()
{
    Textures.clear();
    TextureNames.clear();
    DirtyMaterials.clear();
    Meshes.clear();
    Descriptions.clear();
    Instances.clear();
    Emitters.clear();
    DescriptionBuffer.Destroy();
}

namespace
{

void ModelData::ReadAssimpFile(const std::string& Path, const mat4& M)
{
    printf("ReadAssimpFile File:  %s \n", Path.c_str());

    aiMatrix4x4 ModelTr(M[0][0], M[1][0], M[2][0], M[3][0], M[0][1], M[1][1], M[2][1],
                        M[3][1], M[0][2], M[1][2], M[2][2], M[3][2], M[0][3], M[1][3],
                        M[2][3], M[3][3]);

    Assimp::Importer Importer;
    const aiScene* Aiscene = ImportScene(Importer, Path);

    printf("Assimp mNumMeshes: %d\n", Aiscene->mNumMeshes);
    printf("Assimp mNumMaterials: %d\n", Aiscene->mNumMaterials);
    printf("Assimp mNumTextures: %d\n", Aiscene->mNumTextures);

    for (uint32_t I = 0; I < Aiscene->mNumMaterials; I++)
    {
        ReadMaterial(Aiscene->mMaterials[I], Path);
    }

    RecurseModelNodes(this, Aiscene, Aiscene->mRootNode, ModelTr);
}

const aiScene* ImportScene(Assimp::Importer& Importer, const std::string& Path)
{
    // Does the file exist?
    std::ifstream FindIt(Path.c_str());
    if (FindIt.fail())
    {
        RTRT_FATAL("Model file not found: %s", Path.c_str());
    }

    // Invoke assimp to read the file.
    printf("Assimp %d.%d Reading %s\n", aiGetVersionMajor(), aiGetVersionMinor(),
           Path.c_str());
    const aiScene* Aiscene = Importer.ReadFile(
        Path.c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals);

    if (!Aiscene)
    {
        RTRT_FATAL("Assimp failed to read %s: %s", Path.c_str(),
                   Importer.GetErrorString());
    }

    if (!Aiscene->mRootNode)
    {
        RTRT_FATAL("Model %s has no root node", Path.c_str());
    }
    return Aiscene;
}

void ModelData::ReadMaterial(const aiMaterial* Mtl, const std::string& Path)
{
    aiString Name;
    Mtl->Get(AI_MATKEY_NAME, Name);
    aiColor3D Emit(0.f, 0.f, 0.f);
    aiColor3D Diff(0.f, 0.f, 0.f), Spec(0.f, 0.f, 0.f);
    float Alpha = 20.0;
    bool He = Mtl->Get(AI_MATKEY_COLOR_EMISSIVE, Emit);
    bool Hd = Mtl->Get(AI_MATKEY_COLOR_DIFFUSE, Diff);
    bool Hs = Mtl->Get(AI_MATKEY_COLOR_SPECULAR, Spec);
    bool Ha = Mtl->Get(AI_MATKEY_SHININESS, &Alpha, nullptr);
    aiColor3D Trans;
    bool Ht = Mtl->Get(AI_MATKEY_COLOR_TRANSPARENT, Trans);

    MaterialNames.emplace_back(Name.C_Str());
    Material Newmat;
    // An emitter
    if (!Emit.IsBlack())
    {
        // An emitter needs (1,1,1), else black screen!  WTF???
        Newmat.Diffuse = {1, 1, 1};
        Newmat.Specular = {0, 0, 0};
        Newmat.Shininess = 0.0;
        Newmat.Emission = {Emit.r, Emit.g, Emit.b};
        Newmat.TextureId = -1;
    }

    else
    {
        vec3 Kd(0.5f, 0.5f, 0.5f);
        vec3 Ks(0.03f, 0.03f, 0.03f);
        if (AI_SUCCESS == Hd)
        {
            Kd = vec3(Diff.r, Diff.g, Diff.b);
        }
        if (AI_SUCCESS == Hs)
        {
            Ks = vec3(Spec.r, Spec.g, Spec.b);
        }
        Newmat.Diffuse = {Kd[0], Kd[1], Kd[2]};
        Newmat.Specular = {Ks[0], Ks[1], Ks[2]};
        // Phong exponent -> GGX roughness
        Newmat.Shininess = sqrtf(2.0f / (2.0f + Alpha));
        Newmat.Emission = {0, 0, 0};
        Newmat.TextureId = -1;
    }

    aiString TexPath;
    if (AI_SUCCESS == Mtl->GetTexture(aiTextureType_DIFFUSE, 0, &TexPath))
    {
        fs::path FullPath = Path;
        FullPath.replace_filename(TexPath.C_Str());
        printf("Texture: %ls\n", FullPath.c_str());
        Newmat.TextureId = static_cast<int>(Textures.size());
        auto Xxx = FullPath.u8string();
        Textures.push_back(std::string(Xxx));
    }

    Materials.push_back(Newmat);
}

// Recursively traverses the assimp node hierarchy, accumulating
// modeling transformations, and creating and transforming any meshes
// found.  Meshes comming from assimp can have associated surface
// properties, so each mesh *copies* the current BRDF as a starting
// point and modifies it from the assimp data structure.
void RecurseModelNodes(ModelData* Meshdata, const aiScene* Aiscene, const aiNode* Node,
                       const aiMatrix4x4& ParentTr, const int Level)
{
    // Print line with indentation to show structure of the model node hierarchy.

    // Accumulating transformations while traversing down the hierarchy.
    aiMatrix4x4 ChildTr = ParentTr * Node->mTransformation;
    // Really should be inverse-transpose for full generality
    aiMatrix3x3 NormalTr = aiMatrix3x3(ChildTr);

    // Loop through this node's meshes
    for (unsigned int M = 0; M < Node->mNumMeshes; ++M)
    {
        aiMesh* Aimesh = Aiscene->mMeshes[Node->mMeshes[M]];

        // Loop through all vertices and record the
        // vertex/normal/texture/tangent data with the node's model
        // transformation applied.
        uint32_t FaceOffset = static_cast<uint32_t>(Meshdata->Vertices.size());
        for (unsigned int T = 0; T < Aimesh->mNumVertices; ++T)
        {
            aiVector3D Aipnt = ChildTr * Aimesh->mVertices[T];
            aiVector3D Ainrm = Aimesh->HasNormals() ? NormalTr * Aimesh->mNormals[T]
                                                    : aiVector3D(0, 0, 1);
            aiVector3D Aitex = Aimesh->HasTextureCoords(0) ? Aimesh->mTextureCoords[0][T]
                                                           : aiVector3D(0, 0, 0);
            aiVector3D Aitan = Aimesh->HasTangentsAndBitangents()
                                   ? NormalTr * Aimesh->mTangents[T]
                                   : aiVector3D(1, 0, 0);

            Meshdata->Vertices.push_back({{Aipnt.x, Aipnt.y, Aipnt.z},
                                          {Ainrm.x, Ainrm.y, Ainrm.z},
                                          {Aitex.x, Aitex.y}});
        }

        // Loop through all faces, recording indices
        for (unsigned int T = 0; T < Aimesh->mNumFaces; ++T)
        {
            aiFace* Aiface = &Aimesh->mFaces[T];
            for (uint32_t I = 2; I < Aiface->mNumIndices; I++)
            {
                Meshdata->MatIndx.push_back(Aimesh->mMaterialIndex);
                Meshdata->Indicies.push_back(Aiface->mIndices[0] + FaceOffset);
                Meshdata->Indicies.push_back(Aiface->mIndices[I - 1] + FaceOffset);
                Meshdata->Indicies.push_back(Aiface->mIndices[I] + FaceOffset);
            }
        };
    }

    // Recurse onto this node's children
    for (unsigned int I = 0; I < Node->mNumChildren; ++I)
    {
        RecurseModelNodes(Meshdata, Aiscene, Node->mChildren[I], ChildTr, Level + 1);
    }
}

}  // namespace

}  // namespace rtrt
