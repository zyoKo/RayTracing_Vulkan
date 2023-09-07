#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <math.h>

#include "vkapp.h"
#define SHADOW
#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
using namespace glm;

#include "app.h"
#include "shaders/shared_structs.h"

#define GROUP_SIZE 128


void VkApp::createDenoiseBuffer()
{
    m_denoiseBuffer = createBufferImage(windowSize);
    transitionImageLayout(m_denoiseBuffer.image, VK_FORMAT_R32G32B32A32_SFLOAT,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_GENERAL, 1);
    // @@ destroy m_denoiseBuffer
}

void VkApp::createDenoiseDescriptorSet()
{
    m_denoiseDesc.setBindings(m_device, {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}
        });

    m_denoiseDesc.write(m_device, 0, m_scImageBuffer.Descriptor());   // The input image
    m_denoiseDesc.write(m_device, 1, m_denoiseBuffer.Descriptor());   // The output image
    m_denoiseDesc.write(m_device, 2, m_rtKdCurrBuffer.Descriptor());  // The color buffer
    m_denoiseDesc.write(m_device, 3, m_rtNdCurrBuffer.Descriptor());  // The normal:depth buffer
    // @@ destroy m_denoiseDesc
}

void VkApp::createDenoiseCompPipeline()
{
    // pushing time
    VkPushConstantRange pc_info = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantDenoise)};
    VkPipelineLayoutCreateInfo plCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plCreateInfo.setLayoutCount         = 1;
    plCreateInfo.pSetLayouts            = &m_denoiseDesc.descSetLayout;
    plCreateInfo.pushConstantRangeCount = 1;
    plCreateInfo.pPushConstantRanges    = &pc_info;
    vkCreatePipelineLayout(m_device, &plCreateInfo, nullptr, &m_denoiseCompPipelineLayout);
  
    VkComputePipelineCreateInfo cpCreateInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpCreateInfo.layout = m_denoiseCompPipelineLayout;

    cpCreateInfo.stage = createShaderStageInfo(loadFile("spv/denoise.comp.spv"),
                                               VK_SHADER_STAGE_COMPUTE_BIT);
    vkCreateComputePipelines(m_device, {}, 1, &cpCreateInfo, nullptr, &m_denoisePipeline);
    vkDestroyShaderModule(m_device, cpCreateInfo.stage.module, nullptr);

    // @@ destroy m_denoiseCompPipelineLayout
    // @@ destroy m_denoisePipeline
}

void VkApp::denoise()
{
    // Wait for RT to finish
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageMemoryBarrier    imgMemBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imgMemBarrier.srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT;
    imgMemBarrier.dstAccessMask    = VK_ACCESS_SHADER_READ_BIT;
    imgMemBarrier.image            = m_scImageBuffer.image;
    imgMemBarrier.oldLayout        = VK_IMAGE_LAYOUT_GENERAL;
    imgMemBarrier.newLayout        = VK_IMAGE_LAYOUT_GENERAL;
    imgMemBarrier.subresourceRange = range;

    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 0, nullptr, 1, &imgMemBarrier);

    // Project 6 De-noise
    m_pcDenoise.normFactor = 0.003f;
    m_pcDenoise.depthFactor = 0.007f;

    int stepwidth = 1;
    for(int a = 0; a < m_num_atrous_iterations; a++) 
    {
        // Tell the A-Trous algorithm its "hole" size
        m_pcDenoise.stepwidth = stepwidth;
        stepwidth *= 2;

        // Select the compute shader, and its descriptor set and push constant
        vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_denoisePipeline);
        vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_denoiseCompPipelineLayout, 0, 1,
                                &m_denoiseDesc.descSet, 0, nullptr);
        vkCmdPushConstants(m_commandBuffer, m_denoiseCompPipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantDenoise),
                           &m_pcDenoise);

        // Dispatch the shader in batches of 128x1 (WHY???)
        // This MUST match the shaders's line:
        //    layout(local_size_x=GROUP_SIZE, local_size_y=1, local_size_z=1) in;
        vkCmdDispatch(m_commandBuffer,
                      (windowSize.width + GROUP_SIZE-1) / GROUP_SIZE,
                      windowSize.height, 1);

        // Wait until denoise shader is done writing to m_denoiseBuffer
        imgMemBarrier.image = m_denoiseBuffer.image;
        vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_DEPENDENCY_DEVICE_GROUP_BIT,
                             0, nullptr, 0, nullptr, 1, &imgMemBarrier);

        // @@ Copy the denoised results (in m_denoiseBuffer) back to
        // the input buffer (m_scImageBuffer) for the next denoising
        // loop pass.  See VkApp::raytrace for 4 examples of using
        // VkApp::CmdCopyImage to copy an image.
        CmdCopyImage(m_denoiseBuffer, m_scImageBuffer);
    }
}
