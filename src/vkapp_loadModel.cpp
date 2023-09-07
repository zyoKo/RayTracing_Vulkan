//////////////////////////////////////////////////////////////////////
// Uses the ASSIMP library to read mesh models in of 30+ file types
// into a structure suitable for the raytracer.
////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <math.h>

#include <filesystem>
namespace fs = std::filesystem;

#include "vkapp.h"

#include <assimp/Importer.hpp>
#include <assimp/version.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
using namespace glm;

#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#include "app.h"
#include "shaders/shared_structs.h"

// Local objects and procedures defined and used here:
struct ModelData
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indicies;
    std::vector<Material> materials;
    std::vector<int32_t>     matIndx;
    std::vector<std::string> textures;

    void readAssimpFile(const std::string& path, const mat4& M);
};

void recurseModelNodes(ModelData* meshdata,
                       const  aiScene* aiscene,
                       const  aiNode* node,
                       const aiMatrix4x4& parentTr,
                       const int level=0);


// Returns an address (as VkDeviceAddress=uint64_t) of a buffer on the GPU.
VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
    VkBufferDeviceAddressInfo info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    info.buffer                    = buffer;
    return vkGetBufferDeviceAddress(device, &info);
}

void VkApp::myloadModel(const std::string& filename, glm::mat4 transform)
{
    ModelData meshdata;
    meshdata.readAssimpFile(filename, glm::mat4(1.0f));

    printf("vertices: %zd\n", meshdata.vertices.size());
    printf("indices: %zd (%zd)\n", meshdata.indicies.size(), meshdata.indicies.size()/3);
    printf("materials: %zd\n", meshdata.materials.size());
    printf("matIndx: %zd\n", meshdata.matIndx.size());
    printf("textures: %zd\n", meshdata.textures.size());
    
    // @@ Go though the list of meshdata.materials, find the ones that
    // are emitters, and scale the emission up by a factor of 5.  The
    // model's original values produce very dark image.  We will have
    // a better way to accomplish this later.
    //
    // Hint: meshdata.materials[i].emission is a vec3 color of light emitted.
    for (auto& material : meshdata.materials) {
        if (material.emission != vec3(0))
	        material.emission *= 2.5f;
    }

    // @@ The raytracer will eventually need a list of lights, that is
    // a list of triangle indices whose associated material type has a
    // non-zero emission vec3.  Create such a list.  The vkapp.h header
    // file has no data member for this, so create your own.
    //
    // Hint: Triangle i has
    //   vertices in meshdata.vertices, indexed by [3*i], [3*i+1], [3*i+2]
    //   and a material in meshdata.materials, indexed by meshdata.matIndx[i]
    //for (auto& material : meshdata.materials)
    for (int i = 0; i < meshdata.materials.size(); ++i)
    {
	    if (meshdata.materials[i].emission != vec3(0.0f))
	    {
            Emitter emitter;
            emitter.v0 = meshdata.vertices[meshdata.indicies[i * 3]].pos;
            emitter.v1 = meshdata.vertices[meshdata.indicies[3 * i + 1]].pos;
            emitter.v2 = meshdata.vertices[meshdata.indicies[3 * i + 2]].pos;
            emitter.emission = 4.0f * meshdata.materials[i].emission;
            emitter.index = i;

	    	lightList.push_back(emitter);
	    }
    }
    
    ObjData object;
    object.nbIndices  = static_cast<uint32_t>(meshdata.indicies.size());
    object.nbVertices = static_cast<uint32_t>(meshdata.vertices.size());

    // Create the buffers on Device and copy vertices, indices and materials
    VkCommandBuffer    cmdBuf = createTempCmdBuffer();

    VkBufferUsageFlags flag = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VkBufferUsageFlags rtFlags = flag
        | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
  
    object.vertexBuffer = createStagedBufferWrap(cmdBuf, meshdata.vertices,
                                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rtFlags);
    object.indexBuffer = createStagedBufferWrap(cmdBuf, meshdata.indicies,
                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rtFlags);
    object.matColorBuffer = createStagedBufferWrap(cmdBuf, meshdata.materials, flag);
    object.matIndexBuffer = createStagedBufferWrap(cmdBuf, meshdata.matIndx, flag);
  
    submitTempCmdBuffer(cmdBuf);
    
    // Creates all textures on the GPU
    const auto txtOffset = static_cast<uint32_t>(m_objText.size());  // Offset is current size
    for(const auto& texName : meshdata.textures)
        m_objText.push_back(createTextureImage(texName));

    // Assuming one instance of an object with its supplied transform.
    // Could provide multiple transform here to make a vector of instances of this object.
    ObjInst instance;
    instance.transform = transform;
    instance.objIndex  = static_cast<uint32_t>(m_objData.size()); // Index of current object
    m_objInst.push_back(instance);

    // Creating information for device access
    ObjDesc desc;
    desc.txtOffset            = txtOffset;
    desc.vertexAddress        = getBufferDeviceAddress(m_device, object.vertexBuffer.buffer);
    desc.indexAddress         = getBufferDeviceAddress(m_device, object.indexBuffer.buffer);
    desc.materialAddress      = getBufferDeviceAddress(m_device, object.matColorBuffer.buffer);
    desc.materialIndexAddress = getBufferDeviceAddress(m_device, object.matIndexBuffer.buffer);

    m_objData.emplace_back(object);
    m_objDesc.emplace_back(desc);

    // @@ At shutdown:
    //   Destroy all textures with:  for (t:m_objText) t.destroy(m_device); 
    //   Destroy all buffers with:   for (ob:objDesc) ob.destroy(m_device);
}

void ModelData::readAssimpFile(const std::string& path, const mat4& M)
{
    printf("ReadAssimpFile File:  %s \n", path.c_str());
  
    aiMatrix4x4 modelTr(M[0][0], M[1][0], M[2][0], M[3][0],
                        M[0][1], M[1][1], M[2][1], M[3][1],
                        M[0][2], M[1][2], M[2][2], M[3][2],
                        M[0][3], M[1][3], M[2][3], M[3][3]);

    // Does the file exist?
    std::ifstream find_it(path.c_str());
    if (find_it.fail()) {
        std::cerr << "File not found: "  << path << std::endl;
        exit(-1); }

    // Invoke assimp to read the file.
    printf("Assimp %d.%d Reading %s\n", aiGetVersionMajor(), aiGetVersionMinor(), path.c_str());
    Assimp::Importer importer;
    const aiScene* aiscene = importer.ReadFile(path.c_str(),
                                               aiProcess_Triangulate|aiProcess_GenSmoothNormals);
    
    if (!aiscene) {
        printf("... Failed to read.\n");
        exit(-1); }

    if (!aiscene->mRootNode) {
        printf("Scene has no rootnode.\n");
        exit(-1); }

    printf("Assimp mNumMeshes: %d\n", aiscene->mNumMeshes);
    printf("Assimp mNumMaterials: %d\n", aiscene->mNumMaterials);
    printf("Assimp mNumTextures: %d\n", aiscene->mNumTextures);

    for (int i=0;  i<aiscene->mNumMaterials;  i++) {
        aiMaterial* mtl = aiscene->mMaterials[i];
        aiString name;
        mtl->Get(AI_MATKEY_NAME, name);
        aiColor3D emit(0.f,0.f,0.f); 
        aiColor3D diff(0.f,0.f,0.f), spec(0.f,0.f,0.f); 
        float alpha = 20.0;
        bool he = mtl->Get(AI_MATKEY_COLOR_EMISSIVE, emit);
        bool hd = mtl->Get(AI_MATKEY_COLOR_DIFFUSE, diff);
        bool hs = mtl->Get(AI_MATKEY_COLOR_SPECULAR, spec);
        bool ha = mtl->Get(AI_MATKEY_SHININESS, &alpha, NULL);
        aiColor3D trans;
        bool ht = mtl->Get(AI_MATKEY_COLOR_TRANSPARENT, trans);

        Material newmat;
        if (!emit.IsBlack()) { // An emitter
            newmat.diffuse = {1,1,1};  // An emitter needs (1,1,1), else black screen!  WTF???
            newmat.specular = {0,0,0};
            newmat.shininess = 0.0;
            newmat.emission = {emit.r, emit.g, emit.b};
            newmat.textureId = -1; }
        
        else {
            vec3 Kd(0.5f, 0.5f, 0.5f); 
            vec3 Ks(0.03f, 0.03f, 0.03f);
            if (AI_SUCCESS == hd) Kd = vec3(diff.r, diff.g, diff.b);
            if (AI_SUCCESS == hs) Ks = vec3(spec.r, spec.g, spec.b);
            newmat.diffuse = {Kd[0], Kd[1], Kd[2]};
            newmat.specular = {Ks[0], Ks[1], Ks[2]};
            newmat.shininess = alpha; //sqrtf(2.0f/(2.0f+alpha));
            newmat.emission = {0,0,0};
            newmat.textureId = -1;  }
        
        aiString texPath;
        if (AI_SUCCESS == mtl->GetTexture(aiTextureType_DIFFUSE, 0, &texPath)) {
            fs::path fullPath = path;
            fullPath.replace_filename(texPath.C_Str());
            printf("Texture: %ls\n", fullPath.c_str());
            newmat.textureId = textures.size();
            auto xxx = fullPath.u8string();
            textures.push_back(std::string(xxx));
        }
        
        materials.push_back(newmat);
    }
    
    recurseModelNodes(this, aiscene, aiscene->mRootNode, modelTr);

}

// Recursively traverses the assimp node hierarchy, accumulating
// modeling transformations, and creating and transforming any meshes
// found.  Meshes comming from assimp can have associated surface
// properties, so each mesh *copies* the current BRDF as a starting
// point and modifies it from the assimp data structure.
void recurseModelNodes(ModelData* meshdata,
                       const aiScene* aiscene,
                       const aiNode* node,
                       const aiMatrix4x4& parentTr,
                       const int level)
{
    // Print line with indentation to show structure of the model node hierarchy.
    //for (int i=0;  i<level;  i++) printf("| ");
    //printf("%s \n", node->mName.data);

    // Accumulating transformations while traversing down the hierarchy.
    aiMatrix4x4 childTr = parentTr*node->mTransformation;
    aiMatrix3x3 normalTr = aiMatrix3x3(childTr); // Really should be inverse-transpose for full generality
     
    // Loop through this node's meshes
    for (unsigned int m=0;  m<node->mNumMeshes; ++m) {
        aiMesh* aimesh = aiscene->mMeshes[node->mMeshes[m]];
        //printf("  %d: %d:%d\n", m, aimesh->mNumVertices, aimesh->mNumFaces);

        // Loop through all vertices and record the
        // vertex/normal/texture/tangent data with the node's model
        // transformation applied.
        uint faceOffset = meshdata->vertices.size();
        for (unsigned int t=0;  t<aimesh->mNumVertices;  ++t) {
            aiVector3D aipnt = childTr*aimesh->mVertices[t];
            aiVector3D ainrm = aimesh->HasNormals() ? normalTr*aimesh->mNormals[t] : aiVector3D(0,0,1);
            aiVector3D aitex = aimesh->HasTextureCoords(0) ? aimesh->mTextureCoords[0][t] : aiVector3D(0,0,0);
            aiVector3D aitan = aimesh->HasTangentsAndBitangents() ? normalTr*aimesh->mTangents[t] :  aiVector3D(1,0,0);


            meshdata->vertices.push_back({{aipnt.x, aipnt.y, aipnt.z},
                                          {ainrm.x, ainrm.y, ainrm.z},
                                          {aitex.x, aitex.y}});
        }
        
        // Loop through all faces, recording indices
        for (unsigned int t=0;  t<aimesh->mNumFaces;  ++t) {
            aiFace* aiface = &aimesh->mFaces[t];
            for (int i=2;  i<aiface->mNumIndices;  i++) {
                meshdata->matIndx.push_back(aimesh->mMaterialIndex);
                meshdata->indicies.push_back(aiface->mIndices[0]+faceOffset);
                meshdata->indicies.push_back(aiface->mIndices[i-1]+faceOffset);
                meshdata->indicies.push_back(aiface->mIndices[i]+faceOffset); } }; }


    // Recurse onto this node's children
    for (unsigned int i=0;  i<node->mNumChildren;  ++i)
        recurseModelNodes(meshdata, aiscene, node->mChildren[i], childTr, level+1);
}
