
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>              // for memcpy
#include <vector>
#include <array>
#include <math.h>

#include "vkapp.h"

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
using namespace glm;

#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#include "app.h"
#include "shaders/shared_structs.h"

VkAccessFlags accessFlagsForImageLayout(VkImageLayout layout)
{
    switch(layout)
        {
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return VK_ACCESS_HOST_WRITE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        default:
            return VkAccessFlags();
        }
}

VkPipelineStageFlags pipelineStageForLayout(VkImageLayout layout)
{
    switch(layout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;  // Allow queue other than graphic
            // return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;  // Allow queue other than graphic
            // return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return VK_PIPELINE_STAGE_HOST_BIT;
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        default:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
}


void VkApp::imageLayoutBarrier(VkCommandBuffer cmdbuffer,
                               VkImage image,
                               VkImageLayout oldImageLayout,
                               VkImageLayout newImageLayout,
                               VkImageAspectFlags aspectMask)
{
    VkImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask     = aspectMask;
    subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
    subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
    subresourceRange.baseMipLevel   = 0;
    subresourceRange.baseArrayLayer = 0;
  
    VkImageMemoryBarrier imageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imageMemoryBarrier.oldLayout        = oldImageLayout;
    imageMemoryBarrier.newLayout        = newImageLayout;
    imageMemoryBarrier.image            = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;
    imageMemoryBarrier.srcAccessMask    = accessFlagsForImageLayout(oldImageLayout);
    imageMemoryBarrier.dstAccessMask    = accessFlagsForImageLayout(newImageLayout);
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    VkPipelineStageFlags srcStageMask      = pipelineStageForLayout(oldImageLayout);
    VkPipelineStageFlags destStageMask     = pipelineStageForLayout(newImageLayout);
  
    vkCmdPipelineBarrier(cmdbuffer, srcStageMask, destStageMask, 0,
                         0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}

ImageWrap VkApp::createTextureImage(std::string fileName)
{
    //VkImage& textureImage, VkDeviceMemory& textureImageMemory
    int texWidth, texHeight, texChannels;

    stbi_set_flip_vertically_on_load(true);
    stbi_uc* pixels = stbi_load(fileName.c_str(), &texWidth, &texHeight, &texChannels,
                                STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    BufferWrap staging = createBufferWrap(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(m_device, staging.memory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(m_device, staging.memory);

    stbi_image_free(pixels);

    uint mipLevels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
    
    ImageWrap myImage = createImageWrap(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                  | VK_IMAGE_USAGE_SAMPLED_BIT
                                  | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  mipLevels);

    transitionImageLayout(myImage.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);

    copyBufferToImage(staging.buffer, myImage.image, static_cast<uint32_t>(texWidth),
                      static_cast<uint32_t>(texHeight));

    staging.destroy(m_device);

    generateMipmaps(myImage.image, VK_FORMAT_R8G8B8A8_UNORM, texWidth, texHeight, mipLevels);
    
    myImage.imageView = createImageView(myImage.image, VK_FORMAT_R8G8B8A8_UNORM);
    myImage.sampler = createTextureSampler();
    myImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return myImage;
}

void VkApp::generateMipmaps(VkImage image, VkFormat imageFormat,
                            int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, imageFormat, &formatProperties);
    
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = createTempCmdBuffer();

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
                       image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit,
                       VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    submitTempCmdBuffer(commandBuffer);
}

BufferWrap VkApp::createStagedBufferWrap(const VkCommandBuffer& cmdBuf,
                                         const VkDeviceSize&    size,
                                         const void*            data,
                                         VkBufferUsageFlags     usage)
{
    BufferWrap staging = createBufferWrap(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* dest;
    vkMapMemory(m_device, staging.memory, 0, size, 0, &dest);
    memcpy(dest, data, size);
    vkUnmapMemory(m_device, staging.memory);

    
    BufferWrap bw = createBufferWrap(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    copyBuffer(staging.buffer, bw.buffer, size);

    staging.destroy(m_device);
    
    return bw;
}

BufferWrap VkApp::createBufferWrap(VkDeviceSize size, VkBufferUsageFlags usage,
                                      VkMemoryPropertyFlags properties)
{
    BufferWrap result;
    
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(m_device, &bufferInfo, nullptr, &result.buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, result.buffer, &memRequirements);

    VkMemoryAllocateFlagsInfo memFlags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, nullptr,
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0};

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.pNext = &memFlags;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &result.memory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }
        
    vkBindBufferMemory(m_device, result.buffer, result.memory, 0);

    return result;
}

void VkApp::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer =  createTempCmdBuffer();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    submitTempCmdBuffer(commandBuffer);
}


void VkApp::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = createTempCmdBuffer();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    submitTempCmdBuffer(commandBuffer);
}

void VkApp::transitionImageLayout(VkImage image,
                                        VkFormat format,
                                        VkImageLayout oldLayout,
                                        VkImageLayout newLayout,
                                        uint32_t mipLevels)
{
    VkCommandBuffer commandBuffer = createTempCmdBuffer();

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
                         0, nullptr,    0, nullptr,    1, &barrier);
    
    submitTempCmdBuffer(commandBuffer);
}

VkSampler VkApp::createTextureSampler()
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSampler textureSampler;
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
    return textureSampler;
}

void VkApp::createPostDescriptor()
{
    m_postDesc.setBindings(m_device, {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}
        });
    m_postDesc.write(m_device, 0, m_scImageBuffer.Descriptor());

    // @@ Destroy with m_postDesc.destroy(m_device);
}

void VkApp::createScBuffer()
{
    m_scImageBuffer = createBufferImage(windowSize);

        VkCommandBuffer    cmdBuf = createTempCmdBuffer();
        imageLayoutBarrier(cmdBuf, m_scImageBuffer.image,
                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        submitTempCmdBuffer(cmdBuf);
    
    // @@ Destroy with m_scImageBuffer.destroy(m_device);
}

ImageWrap VkApp::createBufferImage(VkExtent2D& size)
{
    //uint mipLevels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
    uint mipLevels = 1;

    ImageWrap myImage = createImageWrap(size.width, size.height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                  | VK_IMAGE_USAGE_SAMPLED_BIT
                                  | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                  | VK_IMAGE_USAGE_STORAGE_BIT
                                  | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  mipLevels);

    myImage.imageView = createImageView(myImage.image, VK_FORMAT_R32G32B32A32_SFLOAT);
    myImage.sampler = createTextureSampler();
    myImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    return myImage;
}

// The scanline renderpass outputs to m_scImageBuffer (as wrapped by m_scanlineFramebuffer)
void VkApp::createScanlineRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format =  VK_FORMAT_X8_D24_UNORM_PACK32;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    
    std::array<VkAttachmentDescription, 2> attachmentsDsc = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentsDsc.size());
    renderPassInfo.pAttachments = attachmentsDsc.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_scanlineRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create scanline render pass!");
    }

    std::vector<VkImageView> attachments = {m_scImageBuffer.imageView, m_depthImage.imageView};

    VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    info.renderPass      = m_scanlineRenderPass;
    info.attachmentCount = attachments.size();
    info.pAttachments    = attachments.data();
    info.width           = windowSize.width;
    info.height          = windowSize.height;
    info.layers          = 1;
    vkCreateFramebuffer(m_device, &info, nullptr, &m_scanlineFramebuffer);

    // @@ Destroy with vkDestroyRenderPass(m_device, m_scanlineRenderPass, nullptr);
    // @@ Destroy with vkDestroyFramebuffer(m_device, m_scanlineFramebuffer, nullptr);
}

void VkApp::createScDescriptorSet()
{
    auto nbTxt = static_cast<uint32_t>(m_objText.size());

    m_scDesc.setBindings(m_device, {
            {ScBindings::eMatrices, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR},
            {ScBindings::eObjDescs, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
                | VK_SHADER_STAGE_RAYGEN_BIT_KHR},
            {ScBindings::eTextures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nbTxt,
                VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR}
        });
              
    m_scDesc.write(m_device, ScBindings::eMatrices, m_matrixBW.buffer);
    m_scDesc.write(m_device, ScBindings::eObjDescs, m_objDescriptionBW.buffer);
    m_scDesc.write(m_device, ScBindings::eTextures, m_objText);    

    // @@ Destroy with m_scDesc.destroy(m_device);
}

void VkApp::createScPipeline()
{
    VkPushConstantRange pushConstantRanges = {
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantRaster)};

    // Creating the Pipeline Layout
    VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.setLayoutCount         = 1;
    createInfo.pSetLayouts            = &m_scDesc.descSetLayout;
    createInfo.pushConstantRangeCount = 1;
    createInfo.pPushConstantRanges    = &pushConstantRanges;
    vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_scanlinePipelineLayout);


    VkShaderModule vertShaderModule = createShaderModule(loadFile("spv/scanline.vert.spv"));
    VkShaderModule fragShaderModule = createShaderModule(loadFile("spv/scanline.frag.spv"));

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkVertexInputBindingDescription bindingDescription
        {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, pos))},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, nrm))},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, texCoord))}};

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        
    vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) windowSize.width;
    viewport.height = (float) windowSize.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = VkExtent2D{windowSize.width, windowSize.height};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE; //??
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;// BEWARE!!  NECESSARY!!
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = m_scanlinePipelineLayout;
    pipelineInfo.renderPass = m_scanlineRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_scanlinePipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create scanline pipeline!");
    }

    // Done with the temporary spv shader modules.
    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    
    // @@ To destroy:  vkDestroyPipelineLayout(m_device, m_scanlinePipelineLayout, nullptr);
    // @@  and:        vkDestroyPipeline(m_device, m_scanlinePipeline, nullptr);
}

// Create a Vulkan buffer to hold the camera matrices, products and inverses.
// Will be included in a descriptor set for use in shaders.
void VkApp::createMatrixBuffer()
{
    m_matrixBW = createBufferWrap(sizeof(MatrixUniforms),
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                               | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // @@ Destroy with m_matrixBW.destroy(m_device);
}

// Create a Vulkan buffer containing pointers to all object buffers
// (vertex, triangle indices, materials, and material indices. Will be
// included in a descriptor set for use in shaders.
void VkApp::createObjDescriptionBuffer()
{
    VkCommandBuffer cmdBuf = createTempCmdBuffer();
    m_objDescriptionBW  = createStagedBufferWrap(cmdBuf, m_objDesc,
                                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    submitTempCmdBuffer(cmdBuf);
    // @@ Destroy with m_objDescriptionBW.destroy(m_device);
}

void VkApp::rasterize()
{
    VkDeviceSize offset{0};
    
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0,0,0,1}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo _i{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    _i.clearValueCount = 2;
    _i.pClearValues    = clearValues.data();
    _i.renderPass      = m_scanlineRenderPass;
    _i.framebuffer     = m_scanlineFramebuffer;
    _i.renderArea      = {{0, 0}, windowSize};
    vkCmdBeginRenderPass(m_commandBuffer, &_i, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_scanlinePipeline);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_scanlinePipelineLayout, 0, 1, &m_scDesc.descSet, 0, nullptr);

    for(const ObjInst& inst : m_objInst) {
        auto& object            = m_objData[inst.objIndex];
        
        // Information pushed at each draw call
        PushConstantRaster pcRaster{
            inst.transform,      // Object's instance transform.
            {0.5f, 2.5f, 3.0f},  // light position;  Should not be hard-coded here!
            inst.objIndex,       // instance Id
            1.0f                 // light intensity;  Should not be hard-coded here!
        };
        
        pcRaster.objIndex    = inst.objIndex;  // Telling which object is drawn
        pcRaster.modelMatrix = inst.transform;

        vkCmdPushConstants(m_commandBuffer, m_scanlinePipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(PushConstantRaster), &pcRaster);
        vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &object.vertexBuffer.buffer, &offset);
        vkCmdBindIndexBuffer(m_commandBuffer, object.indexBuffer.buffer, 0,
                             VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(m_commandBuffer, object.nbIndices, 1, 0, 0, 0); }
    
    vkCmdEndRenderPass(m_commandBuffer);
}


void VkApp::updateCameraBuffer()
{
    // Prepare new UBO contents on host.
    const float    aspectRatio = windowSize.width / static_cast<float>(windowSize.height);
    MatrixUniforms hostUBO     = {};

    glm::mat4    view = app->myCamera.view(glfwGetTime());
    glm::mat4    proj = app->myCamera.perspective(aspectRatio);
  
    hostUBO.priorViewProj = m_priorViewProj;
    hostUBO.viewProj    = proj * view;
    m_priorViewProj       = hostUBO.viewProj;
    hostUBO.viewInverse = glm::inverse(view);
    hostUBO.projInverse = glm::inverse(proj);

    // UBO on the device, and what stages access it.
    VkBuffer deviceUBO      = m_matrixBW.buffer;
    auto     uboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                            | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

    // Ensure that the modified UBO is not visible to previous frames.
    VkBufferMemoryBarrier beforeBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    beforeBarrier.buffer        = deviceUBO;
    beforeBarrier.offset        = 0;
    beforeBarrier.size          = sizeof(hostUBO);
    vkCmdPipelineBarrier(m_commandBuffer, uboUsageStages, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                         nullptr, 1, &beforeBarrier, 0, nullptr);


    // Schedule the host-to-device upload. (hostUBO is copied into the cmd
    // buffer so it is okay to deallocate when the function returns).
    vkCmdUpdateBuffer(m_commandBuffer, m_matrixBW.buffer, 0, sizeof(MatrixUniforms), &hostUBO);

    // Making sure the updated UBO will be visible.
    VkBufferMemoryBarrier afterBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    afterBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    afterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    afterBarrier.buffer        = deviceUBO;
    afterBarrier.offset        = 0;
    afterBarrier.size          = sizeof(hostUBO);
    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, uboUsageStages,
                         VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                         nullptr, 1, &afterBarrier, 0, nullptr);
}
