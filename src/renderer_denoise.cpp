#include "renderer.h"

#include "shaders/shared_structs.h"
#include "vk_check.h"

namespace rtrt
{

namespace
{

constexpr uint32_t kGroupSize = 128;

}  // namespace

void Renderer::CreateDenoiseBuffer()
{
    DenoiseBuffer = CreateBufferImage(MainSwapchain->GetExtent());
    DenoiseBuffer.TransitionLayout(MainCmdPool, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL);
}

void Renderer::CreateDenoiseDescriptorSet()
{
    DescriptorBuilder()
        .AddStorageImage(VK_SHADER_STAGE_COMPUTE_BIT)
        .AddStorageImage(VK_SHADER_STAGE_COMPUTE_BIT)
        .AddStorageImage(VK_SHADER_STAGE_COMPUTE_BIT)
        .AddStorageImage(VK_SHADER_STAGE_COMPUTE_BIT)
        .Build(GpuDevice->GetHandle(), DenoiseDesc);

    // The input image
    DenoiseDesc.Set.Write(0, ScImageBuffer.Descriptor());
    // The output image
    DenoiseDesc.Set.Write(1, DenoiseBuffer.Descriptor());
    // The color buffer
    DenoiseDesc.Set.Write(2, RtKdCurrBuffer.Descriptor());
    // The normal:depth buffer
    DenoiseDesc.Set.Write(3, RtNdCurrBuffer.Descriptor());
}

void Renderer::CreateDenoiseCompPipeline()
{
    VkPushConstantRange PcInfo = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                  sizeof(PushConstantDenoise)};
    DenoiseCompPipelineLayout.Create(GpuDevice->GetHandle(),
                                     {DenoiseDesc.Layout.GetHandle()}, {PcInfo});

    VkComputePipelineCreateInfo CpCreateInfo{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    CpCreateInfo.layout = DenoiseCompPipelineLayout.GetHandle();

    Shader Comp;
    Comp.Load(GpuDevice->GetHandle(), "spv/denoise.comp.spv",
              "spv/denoise.comp.slang.spv", UseSlang);
    CpCreateInfo.stage = Comp.StageInfo(VK_SHADER_STAGE_COMPUTE_BIT);
    DenoisePipeline.CreateCompute(GpuDevice->GetHandle(), CpCreateInfo);
}

void Renderer::Denoise()
{
    // Wait for RT to finish
    VkImageSubresourceRange Range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageMemoryBarrier ImgMemBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    ImgMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    ImgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    ImgMemBarrier.image = ScImageBuffer.GetHandle();
    ImgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    ImgMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    ImgMemBarrier.subresourceRange = Range;

    vkCmdPipelineBarrier(FrameCmd.GetHandle(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 0, nullptr, 1,
                         &ImgMemBarrier);

    // A-trous wavelet denoise
    int Stepwidth = 1;
    for (int A = 0; A < Settings.NumAtrousIterations; A++)
    {
        // Tell the A-Trous algorithm its "hole" size
        PcDenoise.StepWidth = Stepwidth;
        Stepwidth *= 2;

        // Select the compute shader, and its descriptor set and push constant
        vkCmdBindPipeline(FrameCmd.GetHandle(), VK_PIPELINE_BIND_POINT_COMPUTE,
                          DenoisePipeline.GetHandle());
        vkCmdBindDescriptorSets(FrameCmd.GetHandle(), VK_PIPELINE_BIND_POINT_COMPUTE,
                                DenoiseCompPipelineLayout.GetHandle(), 0, 1,
                                &DenoiseDesc.Set.GetHandle(), 0, nullptr);
        vkCmdPushConstants(FrameCmd.GetHandle(), DenoiseCompPipelineLayout.GetHandle(),
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantDenoise),
                           &PcDenoise);

        // One 128x1 workgroup per row segment; kGroupSize must match
        // local_size_x in denoise.comp.
        vkCmdDispatch(FrameCmd.GetHandle(),
                      (MainSwapchain->GetExtent().width + kGroupSize - 1) / kGroupSize,
                      MainSwapchain->GetExtent().height, 1);

        // Wait until denoise shader is done writing to DenoiseBuffer
        ImgMemBarrier.image = DenoiseBuffer.GetHandle();
        vkCmdPipelineBarrier(FrameCmd.GetHandle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 0, nullptr, 1,
                             &ImgMemBarrier);

        // Feed this pass's output back in as the next pass's input
        CmdCopyImage(DenoiseBuffer, ScImageBuffer);
    }
}

}  // namespace rtrt
