#ifndef COMMON_HOST_DEVICE
#define COMMON_HOST_DEVICE

#ifdef __cplusplus
#include <glm/glm.hpp>
// GLSL Type
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = unsigned int;
#endif

// clang-format off
#ifdef __cplusplus // Descriptor binding helper for C++ and GLSL
#define START_ENUM(a) enum a {
#define END_ENUM() }
#else
#define START_ENUM(a)  const uint
#define END_ENUM() 
#endif

START_ENUM(ScBindings)
eMatrices = 0,  // Global uniform containing camera matrices
eObjDescs = 1,  // Access to the object descriptions
eTextures = 2   // Access to textures
END_ENUM();

START_ENUM(RtBindings)
eTlas = 0,  // Top-level acceleration structure
eOutImage = 1,   // Ray tracer output image
eColorHistoryImage = 2
END_ENUM();
// clang-format on



// Information of a obj model when referenced in a shader
struct ObjDesc
{
    int      txtOffset;             // Texture index offset in the array of textures
    uint64_t vertexAddress;         // Address of the Vertex buffer
    uint64_t indexAddress;          // Address of the index buffer
    uint64_t materialAddress;       // Address of the material buffer
    uint64_t materialIndexAddress;  // Address of the triangle material index buffer
};

// An emitter
struct Emitter
{
    vec3 v0;
    vec3 v1;
    vec3 v2;
    vec3 emission;
    vec3 normal;
    float area;
    uint index;
};

// Uniform buffer set at each frame
struct MatrixUniforms
{
    mat4 viewProj;      // Camera view * projection
    mat4 priorViewProj; // Camera view * projection
    mat4 viewInverse;   // Camera inverse view matrix
    mat4 projInverse;   // Camera inverse projection matrix
};

// Push constant structure for the raster
struct PushConstantRaster
{
    mat4  modelMatrix;  // matrix of the instance
    vec3  lightPosition;
    uint  objIndex;
    float lightIntensity;
    int   lightType;
};


#ifdef __cplusplus
#define ALIGNAS(N) alignas(N)
#else
#define ALIGNAS(N)
#endif

// For structures used by both C++ and GLSL, byte alignment
// differs between the two languages.  Ints, uints, and floats align
// nicely, but bool's do not.
//  Use ALIGNAS(4) to align bools.
//  Use ALIGNAS(4) or not for ints, uints, and floats as they naturally align to 4 byte boundaries.
//  Use ALIGNAS(16) for vec3 and vec4, and probably mat4 (untested).
// @@ Do a sanity check by storing a known sentinel value in the last
// variable alignmentTest in the C++ program and testing for that
// value in raytrace.gen.  If the test fails fill the screen with an
// error color.
//     For example:  if (pcRay.alignmentTest != 1234) {
//                        imageStore(colCurr, ivec2(gl_LaunchIDEXT.xy), vec4(1,0,0,0));
//                        return; }

// If your alignment is incorrect, the VULKAN validation layer now produces this message:
// VUID-VkRayTracingPipelineCreateInfoKHR-layout-03427(ERROR / SPEC): msgNum: 1749323802 - Validation Error: [ VUID-VkRayTracingPipelineCreateInfoKHR-layout-03427 ] Object 0: handle = 0xd600000000d6, type = VK_OBJECT_TYPE_SHADER_MODULE; Object 1: handle = 0xda00000000da, type = VK_OBJECT_TYPE_PIPELINE_LAYOUT; | MessageID = 0x6844901a | Push constant buffer: member:0 member:13 offset:4 in VK_SHADER_STAGE_RAYGEN_BIT_KHR is out of range in VkPipelineLayout 0xda00000000da[]. The Vulkan spec states: layout must be consistent with all shaders specified in pStages (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkRayTracingPipelineCreateInfoKHR-layout-03427)



// Push constant structure for the ray tracer
struct PushConstantRay
{
    ALIGNAS(4) uint frameSeed;
    ALIGNAS(4) int depth;
    ALIGNAS(4) float rr;
    ALIGNAS(16) vec4 tempLightPos;
    ALIGNAS(16) vec4 tempLightInt;
    ALIGNAS(16) vec4 tempAmbient;
    ALIGNAS(4) bool fullBRDF;
    ALIGNAS(4) bool bilinear;
    ALIGNAS(4) float n_threshold;
    ALIGNAS(4) float d_threshold;
    ALIGNAS(4) bool accumulate;
    ALIGNAS(4) bool useHistory;
    ALIGNAS(4) bool doExplicit;
    ALIGNAS(4) bool clear;
    // @@ Set alignmentTest to a known value in C++;  Test for that value in the shader!
    ALIGNAS(4) int alignmentTest;
};

struct Vertex  // Created by readModel; used in shaders
{
    vec3 pos;
    vec3 nrm;
    vec2 texCoord;
};

struct Material  // Created by readModel; used in shaders
{
    vec3  diffuse;
    vec3  specular;
    vec3  emission;
    float shininess;
    int   textureId;
};


// Push constant structure for the ray tracer
struct PushConstantDenoise
{
    float normFactor;
    float depthFactor;
    //float lumenFactor;

    int  stepwidth;
    //ALIGNAS(4) bool demodulate;
    //ALIGNAS(4) bool splitscreen;
};

struct RayPayload
{
    uint seed;		    // Used in Path Tracing step as random number seed
    bool hit;           // Does the ray intersect anything or not?
    float hitDist;      // Used in the denoising step
    vec3 hitPos;	    // The world coordinates of the hit point.      
    int instanceIndex;  // Index of the object instance hit (we have only one, so =0)
    int primitiveIndex; // Index of the hit triangle primitive within object
    vec3 bc;            // Barycentric coordinates of the hit point within triangle
};

#endif