#ifndef RTRT_SRC_SHADERS_SHARED_STRUCTS_H_
#define RTRT_SRC_SHADERS_SHARED_STRUCTS_H_

#ifdef __cplusplus
#include <glm/glm.hpp>
// GLSL Type
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = unsigned int;
#endif

#ifdef __SLANG__
// Compiled with -matrix-layout-column-major so glm matrices load correctly;
// GLSL's M * v is written mul(M, v) in Slang.
typealias vec2 = float2;
typealias vec3 = float3;
typealias vec4 = float4;
typealias ivec2 = int2;
typealias ivec3 = int3;
typealias mat4 = float4x4;
#endif

// clang-format off
#ifdef __cplusplus // Descriptor binding helper for C++ and Slang
#define RTRT_START_ENUM(a) enum a {
#define RTRT_END_ENUM() }
#else
#define RTRT_START_ENUM(a)  static const uint
#define RTRT_END_ENUM()
#endif

RTRT_START_ENUM(ScBindings)
kMatrices = 0,  // Global uniform containing camera matrices
kObjDescs = 1,  // Access to the object descriptions
kTextures = 2   // Access to textures
RTRT_END_ENUM();

RTRT_START_ENUM(RtBindings)
kTlas = 0,  // Top-level acceleration structure
kOutImage = 1,   // Ray tracer output image
kColorHistoryImage = 2
RTRT_END_ENUM();

RTRT_START_ENUM(ViewModes)
kViewFinal = 0,    // Tone-mapped result
kViewAlbedo = 1,   // First-hit diffuse color
kViewNormal = 2,   // First-hit normal
kViewDepth = 3,    // First-hit view-space depth
kViewSamples = 4   // Accumulated sample count per pixel
RTRT_END_ENUM();
// clang-format on



// Information of a obj model when referenced in a shader
struct ObjDesc
{
    int      TxtOffset;             // Texture index offset in the array of textures
    uint64_t VertexAddress;         // Address of the Vertex buffer
    uint64_t IndexAddress;          // Address of the index buffer
    uint64_t MaterialAddress;       // Address of the material buffer
    uint64_t MaterialIndexAddress;  // Address of the triangle material index buffer
};

// An emitter
struct Emitter
{
    vec3 V0;
    vec3 V1;
    vec3 V2;
    vec3 Emission;
    vec3 Normal;
    float Area;
    uint Index;
};

// Uniform buffer set at each frame
struct MatrixUniforms
{
    mat4 ViewProj;      // Camera view * projection
    mat4 PriorViewProj; // Camera view * projection
    mat4 ViewInverse;   // Camera inverse view matrix
    mat4 ProjInverse;   // Camera inverse projection matrix
};

// Push constant structure for the raster
struct PushConstantRaster
{
    mat4  ModelMatrix;  // matrix of the instance
    vec3  LightPosition;
    uint  ObjIndex;
    float LightIntensity;
    int   LightType;
};


#ifdef __cplusplus
#define RTRT_ALIGNAS(N) alignas(N)
#else
#define RTRT_ALIGNAS(N)
#endif

// For structures used by both C++ and the shaders, byte alignment
// differs between the two languages.  Ints, uints, and floats align
// nicely, but bool's do not.
//  Use RTRT_ALIGNAS(4) to align bools.
//  Use RTRT_ALIGNAS(4) or not for ints, uints, and floats as they naturally align to 4 byte boundaries.
//  Use RTRT_ALIGNAS(16) for vec3 and vec4, and probably mat4 (untested).
// PushConstantRay ends with a sentinel (AlignmentTest) that the C++ side
// sets to 1234 and the ray generation shader checks, filling the screen with an error
// color on mismatch. A misaligned layout also trips validation error
// VUID-VkRayTracingPipelineCreateInfoKHR-layout-03427.

// Push constant structure for the ray tracer
struct PushConstantRay
{
    RTRT_ALIGNAS(4) uint FrameSeed;
    RTRT_ALIGNAS(4) int Depth;
    RTRT_ALIGNAS(4) float Rr;
    RTRT_ALIGNAS(16) vec4 TempLightPos;
    RTRT_ALIGNAS(16) vec4 TempLightInt;
    RTRT_ALIGNAS(16) vec4 TempAmbient;
    RTRT_ALIGNAS(4) bool FullBrdf;
    RTRT_ALIGNAS(4) bool Bilinear;
    RTRT_ALIGNAS(4) float NormalThreshold;
    RTRT_ALIGNAS(4) float DepthThreshold;
    RTRT_ALIGNAS(4) bool Accumulate;
    RTRT_ALIGNAS(4) bool UseHistory;
    RTRT_ALIGNAS(4) bool DoExplicit;
    RTRT_ALIGNAS(4) bool Clear;
    RTRT_ALIGNAS(4) float EmissionScale;
    RTRT_ALIGNAS(4) float MaxSamples;  // Limit on the history's sample count; 0 means no limit
    RTRT_ALIGNAS(4) int AlignmentTest; // Sentinel (1234) for checking host/shader layout agreement
};

struct Vertex  // Created by readModel; used in shaders
{
    vec3 Pos;
    vec3 Nrm;
    vec2 TexCoord;
};

struct Material  // Created by readModel; used in shaders
{
    vec3  Diffuse;
    vec3  Specular;
    vec3  Emission;
    float Shininess;
    int   TextureId;
};


// Push constant structure for the denoiser
struct PushConstantDenoise
{
    float NormFactor;
    float DepthFactor;
    int   StepWidth;
    float AlbedoFactor;
    float ColorFactor;
    int   ClampFireflies;
};

// Push constant structure for the post-processing pass
struct PushConstantPost
{
    float Exposure;
    float Gamma;
    uint  ViewMode;
};

struct RayPayload
{
    uint Seed;		    // Used in Path Tracing step as random number seed
    bool Hit;           // Does the ray intersect anything or not?
    float HitDist;      // Used in the denoising step
    vec3 HitPos;	    // The world coordinates of the hit point.      
    int InstanceIndex;  // Index of the object instance hit (we have only one, so =0)
    int PrimitiveIndex; // Index of the hit triangle primitive within object
    vec3 Bc;            // Barycentric coordinates of the hit point within triangle
};

#endif