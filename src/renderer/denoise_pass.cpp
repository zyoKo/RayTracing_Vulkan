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
    // Wait for RT to finish
    VkImageSubresourceRange Range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageMemoryBarrier ImgMemBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    ImgMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    ImgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    ImgMemBarrier.image = Color.GetHandle();
    ImgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    ImgMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    ImgMemBarrier.subresourceRange = Range;

    vkCmdPipelineBarrier(
        Cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 0, nullptr, 1, &ImgMemBarrier);

    // A-trous wavelet denoise
    int Stepwidth = 1;
    for (int A = 0; A < Iterations; A++)
    {
        // Tell the A-Trous algorithm its "hole" size
        Constants.StepWidth = Stepwidth;
        Stepwidth *= 2;

        // Select the compute shader, and its descriptor set and push constant
        vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          FilterPipeline.GetHandle());
        vkCmdBindDescriptorSets(Cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                FilterLayout.GetHandle(), 0, 1, &Desc.Set.GetHandle(), 0,
                                nullptr);
        vkCmdPushConstants(Cmd, FilterLayout.GetHandle(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(PushConstantDenoise), &Constants);

        // One 128x1 workgroup per row segment
        vkCmdDispatch(Cmd,
                      (Context.Chain->GetExtent().width + kGroupSize - 1) / kGroupSize,
                      Context.Chain->GetExtent().height, 1);

        // Wait until denoise shader is done writing to Output
        ImgMemBarrier.image = Output.GetHandle();
        vkCmdPipelineBarrier(Cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 0, nullptr, 1,
                             &ImgMemBarrier);

        // Feed this pass's output back in as the next pass's input
        CmdCopyImage(Cmd, Output, Color, Context.Chain->GetExtent());
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
