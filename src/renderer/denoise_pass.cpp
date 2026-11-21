#include "renderer/denoise_pass.h"

#include <cstdint>

#include "renderer/render_image.h"
#include "vk/shader.h"

namespace rtrt
{

namespace
{

// Must match local_size_x in denoise.comp
constexpr uint32_t kGroupSize = 128;

void CmdMemoryBarrier(VkCommandBuffer Cmd, VkPipelineStageFlags SrcStage,
                      VkAccessFlags SrcAccess, VkPipelineStageFlags DstStage,
                      VkAccessFlags DstAccess)
{
    VkMemoryBarrier Barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    Barrier.srcAccessMask = SrcAccess;
    Barrier.dstAccessMask = DstAccess;
    vkCmdPipelineBarrier(Cmd, SrcStage, DstStage, 0, 1, &Barrier, 0, nullptr, 0, nullptr);
}

}  // namespace

void DenoisePass::Create(const PassContext& Context, const Image& Color,
                         const Image& Albedo, const Image& NormalDepth)
{
    CreateOutput(Context);

    // Input color, output color, albedo, normal and depth
    DescriptorBuilder()
        .AddStorageImage(VK_SHADER_STAGE_COMPUTE_BIT)
        .AddStorageImage(VK_SHADER_STAGE_COMPUTE_BIT)
        .AddStorageImage(VK_SHADER_STAGE_COMPUTE_BIT)
        .AddStorageImage(VK_SHADER_STAGE_COMPUTE_BIT)
        .Build(Context.Gpu->GetHandle(), Desc);
    WriteDescriptors(Color, Albedo, NormalDepth);

    CreatePipeline(Context);
}

void DenoisePass::ReleaseSized()
{
    Output.Destroy();
}

void DenoisePass::OnResize(const PassContext& Context, const Image& Color,
                           const Image& Albedo, const Image& NormalDepth)
{
    CreateOutput(Context);
    WriteDescriptors(Color, Albedo, NormalDepth);
}

void DenoisePass::RebuildPipeline(const PassContext& Context)
{
    FilterPipeline.Destroy();
    FilterLayout.Destroy();
    CreatePipeline(Context);
}

void DenoisePass::Destroy()
{
    Output.Destroy();
    Desc.Destroy();
    FilterLayout.Destroy();
    FilterPipeline.Destroy();
}

void DenoisePass::Record(VkCommandBuffer Cmd, const PassContext& Context,
                         const Image& Color, int Iterations)
{
    // Color was last written by the ray tracing pass's copy
    CmdMemoryBarrier(Cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                     VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

    // A-trous wavelet denoise
    PushConstantDenoise PassConstants = Constants;
    int Stepwidth = 1;
    for (int A = 0; A < Iterations; A++)
    {
        // Tell the A-Trous algorithm its "hole" size
        PassConstants.StepWidth = Stepwidth;
        Stepwidth *= 2;

        // The colour tolerance halves each pass, as the image gets smoother
        PassConstants.ColorFactor = Constants.ColorFactor / static_cast<float>(1 << A);

        // Select the compute shader, and its descriptor set and push constant
        vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          FilterPipeline.GetHandle());
        vkCmdBindDescriptorSets(Cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                FilterLayout.GetHandle(), 0, 1, &Desc.Set.GetHandle(), 0,
                                nullptr);
        vkCmdPushConstants(Cmd, FilterLayout.GetHandle(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(PushConstantDenoise), &PassConstants);

        // One 128x1 workgroup per row segment
        vkCmdDispatch(Cmd,
                      (Context.Chain->GetExtent().width + kGroupSize - 1) / kGroupSize,
                      Context.Chain->GetExtent().height, 1);

        CmdMemoryBarrier(Cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);

        // Feed this pass's output back in as the next pass's input
        CmdCopyImage(Cmd, Output, Color, Context.Chain->GetExtent());

        // The next pass, or the post pass, reads Color
        CmdMemoryBarrier(
            Cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    }
}

void DenoisePass::CreateOutput(const PassContext& Context)
{
    Output = CreateRenderImage(*Context.Gpu, Context.Chain->GetExtent());
    Output.TransitionLayout(*Context.Pool, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_GENERAL);
}

void DenoisePass::WriteDescriptors(const Image& Color, const Image& Albedo,
                                   const Image& NormalDepth) const
{
    Desc.Set.Write(0, Color.Descriptor());
    Desc.Set.Write(1, Output.Descriptor());
    Desc.Set.Write(2, Albedo.Descriptor());
    Desc.Set.Write(3, NormalDepth.Descriptor());
}

void DenoisePass::CreatePipeline(const PassContext& Context)
{
    VkPushConstantRange PcInfo = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                  sizeof(PushConstantDenoise)};
    FilterLayout.Create(Context.Gpu->GetHandle(), {Desc.Layout.GetHandle()}, {PcInfo});

    VkComputePipelineCreateInfo CpCreateInfo{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    CpCreateInfo.layout = FilterLayout.GetHandle();

    Shader Comp;
    Comp.Load(Context.Gpu->GetHandle(), "spv/denoise.comp.spv",
              "spv/denoise.comp.slang.spv", Context.UseSlang);
    CpCreateInfo.stage = Comp.StageInfo(VK_SHADER_STAGE_COMPUTE_BIT);
    FilterPipeline.CreateCompute(Context.Gpu->GetHandle(), CpCreateInfo);
}

}  // namespace rtrt
