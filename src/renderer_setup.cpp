#include "renderer.h"

#include <array>

#include "vk_check.h"

namespace rtrt
{

void Renderer::DestroyAllVulkanResources()
{
    RTRT_CHECK_VK(vkDeviceWaitIdle(GpuDevice->GetHandle()));

    MainSwapchain->Destroy();
    FrameFence.Destroy();
    MainCmdPool.Destroy();

    DestroyPostResources();
    DestroySceneResources();
    DestroyScanlineResources();
    DestroyRaytracingResources();
    DestroyDenoiseResources();
}

void Renderer::DestroyPostResources()
{
    DepthImage.Destroy();
    ScImageBuffer.Destroy();
    Post.Destroy();
}

void Renderer::DestroySceneResources()
{
    MainScene.Destroy();
    MatrixBw.Destroy();
}

void Renderer::DestroyScanlineResources()
{
    Scanline.Destroy();
    ScDesc.Destroy();
}

void Renderer::DestroyRaytracingResources()
{
    SceneAccel.Destroy();
    RayTrace.Destroy();
}

void Renderer::DestroyDenoiseResources()
{
    Denoiser.Destroy();
}

bool Renderer::RecreateSizedResources()
{
    if (!MainWindow->WaitWhileMinimized())
    {
        return false;
    }

    RTRT_CHECK_VK(vkDeviceWaitIdle(GpuDevice->GetHandle()));
    MainWindow->SetFramebufferResized(false);

    DestroySizedResources();

    CreateSwapchain();
    CreateDepthResource();
    CreateScBuffer();
    Post.OnResize(PassCtx, ScImageBuffer, DepthImage);
    Scanline.OnResize(PassCtx, ScImageBuffer, DepthImage, ScDesc.Layout.GetHandle());
    RayTrace.OnResize(PassCtx);
    Denoiser.OnResize(PassCtx, ScImageBuffer, RayTrace.GetAlbedo(),
                      RayTrace.GetNormalDepth());

    ResetRtAccumulation();
    return true;
}

void Renderer::DestroySizedResources()
{
    Post.ReleaseSized();
    Scanline.ReleaseSized();
    DepthImage.Destroy();
    MainSwapchain->Destroy();

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
    DepthImage.Create(*GpuDevice, MainSwapchain->GetExtent().width,
                      MainSwapchain->GetExtent().height, VK_FORMAT_X8_D24_UNORM_PACK32,
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

    // The last frame may still be using the old pipelines
    RTRT_CHECK_VK(vkDeviceWaitIdle(GpuDevice->GetHandle()));

    Post.RebuildPipeline(PassCtx);

    Scanline.RebuildPipeline(PassCtx, ScDesc.Layout.GetHandle());

    RayTrace.RebuildPipeline(PassCtx, ScDesc.Layout.GetHandle());

    Denoiser.RebuildPipeline(PassCtx);
}

}  // namespace rtrt
