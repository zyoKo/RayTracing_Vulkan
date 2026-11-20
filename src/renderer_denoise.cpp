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
    DenoiseBuffer = CreateBufferImage(WindowSize);
    TransitionImageLayout(DenoiseBuffer.Image, VK_FORMAT_R32G32B32A32_SFLOAT,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
}

void Renderer::CreateDenoiseDescriptorSet()
{
    DenoiseDesc.SetBindings(
        Device, {{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
                 {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
                 {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
                 {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}});

    DenoiseDesc.Write(Device, 0, ScImageBuffer.Descriptor());   // The input image
    DenoiseDesc.Write(Device, 1, DenoiseBuffer.Descriptor());   // The output image
    DenoiseDesc.Write(Device, 2, RtKdCurrBuffer.Descriptor());  // The color buffer
    // The normal:depth buffer
    DenoiseDesc.Write(Device, 3, RtNdCurrBuffer.Descriptor());
}

void Renderer::CreateDenoiseCompPipeline()
{
    VkPushConstantRange PcInfo = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                  sizeof(PushConstantDenoise)};
    VkPipelineLayoutCreateInfo PlCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    PlCreateInfo.setLayoutCount = 1;
    PlCreateInfo.pSetLayouts = &DenoiseDesc.DescSetLayout;
    PlCreateInfo.pushConstantRangeCount = 1;
    PlCreateInfo.pPushConstantRanges = &PcInfo;
    RTRT_CHECK_VK(vkCreatePipelineLayout(Device, &PlCreateInfo, nullptr,
                                         &DenoiseCompPipelineLayout));

    VkComputePipelineCreateInfo CpCreateInfo{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    CpCreateInfo.layout = DenoiseCompPipelineLayout;

    CpCreateInfo.stage = CreateShaderStageInfo(
        LoadShader("spv/denoise.comp.spv", "spv/denoise.comp.slang.spv"),
        VK_SHADER_STAGE_COMPUTE_BIT);
    RTRT_CHECK_VK(vkCreateComputePipelines(Device, {}, 1, &CpCreateInfo, nullptr,
                                           &DenoisePipeline));
    vkDestroyShaderModule(Device, CpCreateInfo.stage.module, nullptr);
}

void Renderer::Denoise()
{
    // Wait for RT to finish
    VkImageSubresourceRange Range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageMemoryBarrier ImgMemBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    ImgMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    ImgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    ImgMemBarrier.image = ScImageBuffer.Image;
    ImgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    ImgMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    ImgMemBarrier.subresourceRange = Range;

    vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 0, nullptr, 1,
                         &ImgMemBarrier);

    // A-trous wavelet denoise
    int Stepwidth = 1;
    for (int A = 0; A < NumAtrousIterations; A++)
    {
        // Tell the A-Trous algorithm its "hole" size
        PcDenoise.StepWidth = Stepwidth;
        Stepwidth *= 2;

        // Select the compute shader, and its descriptor set and push constant
        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, DenoisePipeline);
        vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                DenoiseCompPipelineLayout, 0, 1, &DenoiseDesc.DescSet, 0,
                                nullptr);
        vkCmdPushConstants(CommandBuffer, DenoiseCompPipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantDenoise),
                           &PcDenoise);

        // One 128x1 workgroup per row segment; kGroupSize must match
        // local_size_x in denoise.comp.
        vkCmdDispatch(CommandBuffer, (WindowSize.width + kGroupSize - 1) / kGroupSize,
                      WindowSize.height, 1);

        // Wait until denoise shader is done writing to DenoiseBuffer
        ImgMemBarrier.image = DenoiseBuffer.Image;
        vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 0, nullptr, 1,
                             &ImgMemBarrier);

        // Feed this pass's output back in as the next pass's input
        CmdCopyImage(DenoiseBuffer, ScImageBuffer);
    }
}

}  // namespace rtrt
