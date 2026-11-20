#include "renderer.h"

#include "shaders/shared_structs.h"
#include "vk_check.h"

namespace rtrt
{

namespace
{

constexpr uint32_t GROUP_SIZE = 128;

}  // namespace

void Renderer::createDenoiseBuffer()
{
    m_denoiseBuffer = createBufferImage(windowSize);
    transitionImageLayout(m_denoiseBuffer.image, VK_FORMAT_R32G32B32A32_SFLOAT,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
}

void Renderer::createDenoiseDescriptorSet()
{
    m_denoiseDesc.SetBindings(
        m_device,
        {{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
         {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
         {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
         {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}});

    m_denoiseDesc.Write(m_device, 0, m_scImageBuffer.Descriptor());   // The input image
    m_denoiseDesc.Write(m_device, 1, m_denoiseBuffer.Descriptor());   // The output image
    m_denoiseDesc.Write(m_device, 2, m_rtKdCurrBuffer.Descriptor());  // The color buffer
    // The normal:depth buffer
    m_denoiseDesc.Write(m_device, 3, m_rtNdCurrBuffer.Descriptor());
}

void Renderer::createDenoiseCompPipeline()
{
    VkPushConstantRange pc_info = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   sizeof(PushConstantDenoise)};
    VkPipelineLayoutCreateInfo plCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plCreateInfo.setLayoutCount = 1;
    plCreateInfo.pSetLayouts = &m_denoiseDesc.DescSetLayout;
    plCreateInfo.pushConstantRangeCount = 1;
    plCreateInfo.pPushConstantRanges = &pc_info;
    RTRT_CHECK_VK(vkCreatePipelineLayout(m_device, &plCreateInfo, nullptr,
                                         &m_denoiseCompPipelineLayout));

    VkComputePipelineCreateInfo cpCreateInfo{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpCreateInfo.layout = m_denoiseCompPipelineLayout;

    cpCreateInfo.stage = createShaderStageInfo(
        loadShader("spv/denoise.comp.spv", "spv/denoise.comp.slang.spv"),
        VK_SHADER_STAGE_COMPUTE_BIT);
    RTRT_CHECK_VK(vkCreateComputePipelines(m_device, {}, 1, &cpCreateInfo, nullptr,
                                           &m_denoisePipeline));
    vkDestroyShaderModule(m_device, cpCreateInfo.stage.module, nullptr);
}

void Renderer::denoise()
{
    // Wait for RT to finish
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageMemoryBarrier imgMemBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imgMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imgMemBarrier.image = m_scImageBuffer.image;
    imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgMemBarrier.subresourceRange = range;

    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 0, nullptr, 1,
                         &imgMemBarrier);

    // A-trous wavelet denoise
    int stepwidth = 1;
    for (int a = 0; a < m_num_atrous_iterations; a++)
    {
        // Tell the A-Trous algorithm its "hole" size
        m_pcDenoise.StepWidth = stepwidth;
        stepwidth *= 2;

        // Select the compute shader, and its descriptor set and push constant
        vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          m_denoisePipeline);
        vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_denoiseCompPipelineLayout, 0, 1, &m_denoiseDesc.DescSet,
                                0, nullptr);
        vkCmdPushConstants(m_commandBuffer, m_denoiseCompPipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantDenoise),
                           &m_pcDenoise);

        // One 128x1 workgroup per row segment; GROUP_SIZE must match
        // local_size_x in denoise.comp.
        vkCmdDispatch(m_commandBuffer, (windowSize.width + GROUP_SIZE - 1) / GROUP_SIZE,
                      windowSize.height, 1);

        // Wait until denoise shader is done writing to m_denoiseBuffer
        imgMemBarrier.image = m_denoiseBuffer.image;
        vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 0, nullptr, 1,
                             &imgMemBarrier);

        // Feed this pass's output back in as the next pass's input
        CmdCopyImage(m_denoiseBuffer, m_scImageBuffer);
    }
}

}  // namespace rtrt
