#include "renderer/renderer.h"

#include <array>
#include <cstdlib>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>

#include "renderer/render_image.h"
#include "shaders/shared_structs.h"
#include "vk_check.h"

namespace rtrt
{

bool Renderer::Init(Window& Target, Context& Instance, Device& Gpu, Swapchain& Chain,
                    const Camera& View, bool UseSlangShaders)
{
    MainWindow = &Target;
    GpuContext = &Instance;
    GpuDevice = &Gpu;
    MainSwapchain = &Chain;
    MainCamera = &View;
    UseSlang = UseSlangShaders;
    PassCtx = {GpuDevice, &MainCmdPool, MainSwapchain, {0, 0}, UseSlang};

    CreateCommandPool();

    CreateSwapchain();
    CreateFrameFence();
    PassCtx.Extent = MainSwapchain->GetExtent();
    RequestedExtent = PassCtx.Extent;
    Ui.Create(PassCtx);
    CreateDepthResource();
    CreateScBuffer();
    Post.Create(PassCtx, ScImageBuffer);

    MainScene.LoadModel(*GpuDevice, MainCmdPool, "models/living_room/living_room.obj",
                        glm::mat4(1.0f));

    CreateMatrixBuffer();
    MainScene.CreateDescriptionBuffer(*GpuDevice, MainCmdPool);
    CreateScDescriptorSet();
    Scanline.Create(PassCtx, ScImageBuffer, DepthImage, ScDesc.Layout.GetHandle());

    InitRayTracing();
    CreateRtAccelerationStructure();
    RayTrace.Create(PassCtx, SceneAccel.GetAccelerationStructure(),
                    ScDesc.Layout.GetHandle());

    Denoiser.Create(PassCtx, ScImageBuffer, RayTrace.GetAlbedo(),
                    RayTrace.GetNormalDepth());
    return true;
}

void Renderer::DrawFrame()
{
    const double Start = glfwGetTime();
    if (!PrepareFrame())
    {
        ImGui::EndFrame();
        return;
    }

    FrameCmd.Begin();
    {  // Extra indent for code clarity
        UpdateCameraBuffer();

        // Draw scene
        if (Settings.UseRaytracer)
        {
            RayTrace.Record(FrameCmd.GetHandle(), PassCtx, ScDesc.Set.GetHandle(),
                            ScImageBuffer, Settings.ViewMode, ResetHistory);
            ResetHistory = false;
            FrameCount++;
            if (Settings.DenoiseEnabled && Settings.ViewMode == kViewFinal)
            {
                Denoiser.Record(FrameCmd.GetHandle(), PassCtx, ScImageBuffer,
                                Settings.NumAtrousIterations);
            }
        }
        else
        {
            Scanline.Record(FrameCmd.GetHandle(), PassCtx, ScDesc.Set.GetHandle(),
                            MainScene);
        }

        // The rasterizer has no G-buffer images, so the debug views only apply to the
        // ray tracer.
        const PushConstantPost PcPost{
            Settings.Exposure, Settings.Gamma,
            static_cast<uint32_t>(Settings.UseRaytracer ? Settings.ViewMode
                                                        : kViewFinal)};
        Post.Record(FrameCmd.GetHandle(), PassCtx, PcPost);
        Ui.Record(FrameCmd.GetHandle(), PassCtx);

    }  // Done recording;  Execute!

    FrameCmd.End();

    SubmitFrame();  // Submit for display
    RecordMs = (glfwGetTime() - Start) * 1000.0 - GpuWaitMs;
}

bool Renderer::PrepareFrame()
{
    // Use a fence to wait until the command buffer has finished execution before using it
    // again. This must happen before the acquire, so the previous submit's wait on
    // acquire semaphore is done.
    const double WaitStart = glfwGetTime();
    FrameFence.Wait();
    GpuWaitMs = (glfwGetTime() - WaitStart) * 1000.0;

    if (MainWindow->GetFramebufferResized() && !RecreateSwapchain())
    {
        return false;
    }

    // A suboptimal acquire still signals the acquire semaphore, so that frame is drawn
    // and the swapchain is rebuilt after its present.
    VkResult Result = MainSwapchain->AcquireNextImage();
    while (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if (!RecreateSwapchain())
        {
            return false;
        }
        Result = MainSwapchain->AcquireNextImage();
    }
    CheckVkResult(Result, "vkAcquireNextImageKHR", __FILE__, __LINE__);
    return true;
}

void Renderer::SubmitFrame()
{
    FrameFence.Reset();

    // Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
    const VkPipelineStageFlags WaitStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    const VkSemaphore Acquired = MainSwapchain->GetAcquireSemaphore();
    const VkSemaphore Rendered = MainSwapchain->GetRenderedSemaphore();

    VkSubmitInfo FrameSubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    FrameSubmitInfo.pWaitDstStageMask = &WaitStageMask;
    FrameSubmitInfo.waitSemaphoreCount = 1;
    FrameSubmitInfo.pWaitSemaphores = &Acquired;
    FrameSubmitInfo.signalSemaphoreCount = 1;
    FrameSubmitInfo.pSignalSemaphores = &Rendered;
    FrameSubmitInfo.commandBufferCount = 1;
    const VkCommandBuffer Cmd = FrameCmd.GetHandle();
    FrameSubmitInfo.pCommandBuffers = &Cmd;
    RTRT_CHECK_VK(vkQueueSubmit(GpuDevice->GetQueue(), 1, &FrameSubmitInfo,
                                FrameFence.GetHandle()));

    VkResult Result = MainSwapchain->Present(GpuDevice->GetQueue());
    if (Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR)
    {
        MainWindow->SetFramebufferResized(true);
    }
    else
    {
        CheckVkResult(Result, "vkQueuePresentKHR", __FILE__, __LINE__);
    }
}

void Renderer::DestroyAllVulkanResources()
{
    RTRT_CHECK_VK(vkDeviceWaitIdle(GpuDevice->GetHandle()));

    MainSwapchain->Destroy();
    FrameFence.Destroy();
    MainCmdPool.Destroy();

    Post.Destroy();
    Ui.Destroy();
    Scanline.Destroy();
    RayTrace.Destroy();
    Denoiser.Destroy();
    DepthImage.Destroy();
    ScImageBuffer.Destroy();
    SceneAccel.Destroy();
    ScDesc.Destroy();
    MatrixBw.Destroy();
    MainScene.Destroy();
}

bool Renderer::RecreateSwapchain()
{
    if (!MainWindow->WaitWhileMinimized())
    {
        return false;
    }

    RTRT_CHECK_VK(vkDeviceWaitIdle(GpuDevice->GetHandle()));
    MainWindow->SetFramebufferResized(false);

    Ui.ReleaseSized();
    MainSwapchain->Destroy();
    CreateSwapchain();
    Ui.OnResize(PassCtx);
    return true;
}

void Renderer::ApplyRenderExtent()
{
    const VkExtent2D Current = PassCtx.Extent;
    if (RequestedExtent.width == 0 || RequestedExtent.height == 0 ||
        (RequestedExtent.width == Current.width &&
         RequestedExtent.height == Current.height))
    {
        return;
    }

    RTRT_CHECK_VK(vkDeviceWaitIdle(GpuDevice->GetHandle()));
    DestroyRenderSized();
    PassCtx.Extent = RequestedExtent;
    CreateRenderSized();
    ResetRtAccumulation();
    ViewportVersion++;
}

void Renderer::CreateRenderSized()
{
    CreateDepthResource();
    CreateScBuffer();
    Post.OnResize(PassCtx, ScImageBuffer);
    Scanline.OnResize(PassCtx, ScImageBuffer, DepthImage, ScDesc.Layout.GetHandle());
    RayTrace.OnResize(PassCtx);
    Denoiser.OnResize(PassCtx, ScImageBuffer, RayTrace.GetAlbedo(),
                      RayTrace.GetNormalDepth());
}

void Renderer::DestroyRenderSized()
{
    Post.ReleaseSized();
    Scanline.ReleaseSized();
    DepthImage.Destroy();
    ScImageBuffer.Destroy();
    RayTrace.ReleaseSized();
    Denoiser.ReleaseSized();
}

// Create a command pool, used to allocate command buffers, which in
// turn are use to gather and send commands to the GPU.  The flag
// makes it possible to reuse command buffers.  The queue index
// determines which queue the command buffers can be submitted to.
// Use the command pool to also create a command buffer.
void Renderer::CreateCommandPool()
{
    MainCmdPool.Create(*GpuDevice);
    FrameCmd.Allocate(MainCmdPool);
}

void Renderer::CreateSwapchain()
{
    MainSwapchain->Create(*GpuDevice, GpuContext->GetSurface(), *MainWindow);

    VkCommandBuffer Cmd = MainCmdPool.BeginSingleUse();
    MainSwapchain->RecordInitialTransition(Cmd);
    MainCmdPool.EndSingleUse(Cmd);
}

void Renderer::CreateFrameFence()
{
    FrameFence.Create(*GpuDevice, true);
}

void Renderer::CreateDepthResource()
{
    DepthImage.Create(*GpuDevice, PassCtx.Extent.width, PassCtx.Extent.height,
                      VK_FORMAT_X8_D24_UNORM_PACK32,
                      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    DepthImage.CreateView(VK_FORMAT_X8_D24_UNORM_PACK32, VK_IMAGE_ASPECT_DEPTH_BIT);
}

// Gets a list of memory types supported by the GPU, and search
// through that list for one that matches the requested properties
// flag.  The (only?) two types requested here are:
//
// (1) VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: For the bulk of the memory
// used by the GPU to store things internally.
//
// (2) VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:
// for memory visible to the CPU  for CPU to GPU copy operations.
void Renderer::SetShaderLanguage(bool UseSlangShaders)
{
    if (UseSlangShaders == UseSlang)
    {
        return;
    }
    UseSlang = UseSlangShaders;
    PassCtx.UseSlang = UseSlang;
    ReloadShaders();
}

void Renderer::ReloadShaders()
{
    // The last frame may still be using the old pipelines
    RTRT_CHECK_VK(vkDeviceWaitIdle(GpuDevice->GetHandle()));

    Post.RebuildPipeline(PassCtx);

    Scanline.RebuildPipeline(PassCtx, ScDesc.Layout.GetHandle());

    RayTrace.RebuildPipeline(PassCtx, ScDesc.Layout.GetHandle());

    Denoiser.RebuildPipeline(PassCtx);
}

void Renderer::CreateScBuffer()
{
    ScImageBuffer = CreateRenderImage(*GpuDevice, PassCtx.Extent);

    VkCommandBuffer CmdBuf = MainCmdPool.BeginSingleUse();
    CmdImageBarrier(CmdBuf, ScImageBuffer.GetHandle(), VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_GENERAL);
    MainCmdPool.EndSingleUse(CmdBuf);
}

void Renderer::CreateScDescriptorSet()
{
    auto NbTxt = static_cast<uint32_t>(MainScene.GetTextures().size());

    // In ScBindings order: kMatrices, kObjDescs, kTextures
    DescriptorBuilder()
        .AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddStorageBuffer(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                          VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddCombinedImageSampler(
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, NbTxt)
        .Build(GpuDevice->GetHandle(), ScDesc);

    ScDesc.Set.Write(ScBindings::kMatrices, MatrixBw.GetHandle());
    ScDesc.Set.Write(ScBindings::kObjDescs, MainScene.GetDescriptionBuffer().GetHandle());
    ScDesc.Set.Write(ScBindings::kTextures, MainScene.GetTextures());
}

// Create a Vulkan buffer to hold the camera matrices, products and inverses.
// Will be included in a descriptor set for use in shaders.
void Renderer::CreateMatrixBuffer()
{
    MatrixBw.Create(*GpuDevice, sizeof(MatrixUniforms),
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

// Create a Vulkan buffer containing pointers to all object buffers
// (vertex, triangle indices, materials, and material indices. Will be
// included in a descriptor set for use in shaders.
void Renderer::UpdateCameraBuffer()
{
    // Prepare new UBO contents on host.
    const float AspectRatio =
        PassCtx.Extent.width / static_cast<float>(PassCtx.Extent.height);
    MatrixUniforms HostUBO = {};

    glm::mat4 View = MainCamera->View();
    glm::mat4 Proj = MainCamera->Perspective(AspectRatio);

    HostUBO.PriorViewProj = PriorViewProj;
    HostUBO.ViewProj = Proj * View;
    PriorViewProj = HostUBO.ViewProj;
    HostUBO.ViewInverse = glm::inverse(View);
    HostUBO.ProjInverse = glm::inverse(Proj);

    // UBO on the device, and what stages access it.
    VkBuffer DeviceUBO = MatrixBw.GetHandle();
    auto UboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                          VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

    // Ensure that the modified UBO is not visible to previous frames.
    VkBufferMemoryBarrier BeforeBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    BeforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    BeforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    BeforeBarrier.buffer = DeviceUBO;
    BeforeBarrier.offset = 0;
    BeforeBarrier.size = sizeof(HostUBO);
    vkCmdPipelineBarrier(FrameCmd.GetHandle(), UboUsageStages,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT,
                         0, nullptr, 1, &BeforeBarrier, 0, nullptr);

    // Schedule the host-to-device upload. (hostUBO is copied into the cmd
    // buffer so it is okay to deallocate when the function returns).
    vkCmdUpdateBuffer(FrameCmd.GetHandle(), MatrixBw.GetHandle(), 0,
                      sizeof(MatrixUniforms), &HostUBO);

    // Making sure the updated UBO will be visible.
    VkBufferMemoryBarrier AfterBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    AfterBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    AfterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    AfterBarrier.buffer = DeviceUBO;
    AfterBarrier.offset = 0;
    AfterBarrier.size = sizeof(HostUBO);
    vkCmdPipelineBarrier(FrameCmd.GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                         UboUsageStages, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 1,
                         &AfterBarrier, 0, nullptr);
}

// The values of everything adjustable in the settings panel
void Renderer::SetDefaultSettings()
{
    PushConstantRay& PcRay = RayTrace.GetConstants();
    // Russian roulette survival chance, tested per bounce in the shader
    PcRay.Rr = 0.7f;
    PcRay.Depth = 16;  // Upper limit on path length; 0.7^16 of paths would reach it
    PcRay.EmissionScale = 1.0f;
    PcRay.FullBrdf = true;
    PcRay.UseHistory = true;
    PcRay.NormalThreshold = 0.80f;
    PcRay.DepthThreshold = 0.04f;
    PcRay.MaxSamples = 0.0f;

    Settings.DenoiseEnabled = true;
    Settings.NumAtrousIterations = 5;
    PushConstantDenoise& PcDenoise = Denoiser.GetConstants();
    PcDenoise.NormFactor = 0.003f;
    PcDenoise.DepthFactor = 0.007f;
    PcDenoise.AlbedoFactor = 0.1f;
    PcDenoise.ColorFactor = 0.5f;
    PcDenoise.ClampFireflies = 1;

    Settings.ViewMode = kViewFinal;
    Settings.Exposure = 1.0f;
    Settings.Gamma = 2.2f;

    ResetRtAccumulation();
}

// Makes the next frame discard all temporal history
void Renderer::ResetRtAccumulation()
{
    ResetHistory = true;
    FrameCount = 0;
}

// Initialize ray tracing
void Renderer::InitRayTracing()
{
    SetDefaultSettings();

    // This initializes the acceleration structure helper class
    SceneAccel.Setup(*GpuDevice, MainCmdPool);
}

//----------------------------------------------------------------------------------------
// This descriptor set holds the Acceleration structure and the output image
//
// Pipeline for the ray tracer: all shaders, raygen, chit, miss
//
// Converts a mesh into the ray tracing geometry used to build its BLAS.
BlasInput Renderer::ObjectToVkGeometryKhr(const Mesh& Model)
{
    // BLAS builder requires raw device addresses.
    VkBufferDeviceAddressInfo VertexAddressInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
        Model.VertexBuffer.GetHandle()};
    VkDeviceAddress VertexAddress =
        vkGetBufferDeviceAddress(GpuDevice->GetHandle(), &VertexAddressInfo);

    VkBufferDeviceAddressInfo IndexAddressInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
        Model.IndexBuffer.GetHandle()};
    VkDeviceAddress IndexAddress =
        vkGetBufferDeviceAddress(GpuDevice->GetHandle(), &IndexAddressInfo);

    uint32_t MaxPrimitiveCount = Model.NbIndices / 3;

    // Describe buffer as array of Vertex.
    VkAccelerationStructureGeometryTrianglesDataKHR Triangles{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    Triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
    Triangles.vertexData.deviceAddress = VertexAddress;
    Triangles.vertexStride = sizeof(Vertex);
    // Describe index data (32-bit unsigned int)
    Triangles.indexType = VK_INDEX_TYPE_UINT32;
    Triangles.indexData.deviceAddress = IndexAddress;
    // Indicate identity transform by setting transformData to null device pointer.
    Triangles.maxVertex = Model.NbVertices;

    // Identify the above data as containing opaque triangles.
    VkAccelerationStructureGeometryKHR AsGeom{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    AsGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    AsGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    AsGeom.geometry.triangles = Triangles;

    // The entire array will be used to build the BLAS.
    VkAccelerationStructureBuildRangeInfoKHR Offset;
    Offset.firstVertex = 0;
    Offset.primitiveCount = MaxPrimitiveCount;
    Offset.primitiveOffset = 0;
    Offset.transformOffset = 0;

    // Our blas is made from only one geometry, but could be made of many geometries
    BlasInput Input;
    Input.AsGeometry.emplace_back(AsGeom);
    Input.AsBuildOffsetInfo.emplace_back(Offset);

    return Input;
}

void Renderer::CreateRtAccelerationStructure()
{
    // BLAS - Storing each primitive in a geometry
    std::vector<BlasInput> AllBlas;
    AllBlas.reserve(MainScene.GetMeshes().size());
    for (const Mesh& Obj : MainScene.GetMeshes())
    {
        BlasInput Blas = ObjectToVkGeometryKhr(Obj);
        // We could add more geometry in each BLAS, but we add only one for now
        AllBlas.emplace_back(Blas);
    }

    SceneAccel.BuildBlas(AllBlas,
                         VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    // TLAS
    std::vector<VkAccelerationStructureInstanceKHR> Tlas;
    Tlas.reserve(MainScene.GetInstances().size());
    for (const MeshInstance& Inst : MainScene.GetInstances())
    {
        VkAccelerationStructureInstanceKHR RayInst{};
        // Position of the instance
        RayInst.transform = ToTransformMatrixKhr(Inst.Transform);
        RayInst.instanceCustomIndex = Inst.ObjIndex;
        RayInst.accelerationStructureReference =
            SceneAccel.GetBlasDeviceAddress(Inst.ObjIndex);
        RayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        RayInst.mask = 0xFF;  //  Only be hit if rayMask & instance.mask != 0
        // Use the same hit group for all objects
        RayInst.instanceShaderBindingTableRecordOffset = 0;
        Tlas.emplace_back(RayInst);
    }

    SceneAccel.BuildTlas(Tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                         false, false);

    SceneAccel.DestroyScratchBuffers();
}

}  // namespace rtrt
