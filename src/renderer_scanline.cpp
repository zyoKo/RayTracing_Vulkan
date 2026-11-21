#include "renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#include "shaders/shared_structs.h"
#include "vk_check.h"

namespace rtrt
{

namespace
{

VkAccessFlags AccessFlagsForImageLayout(VkImageLayout Layout)
{
    switch (Layout)
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

VkPipelineStageFlags PipelineStageForLayout(VkImageLayout Layout)
{
    switch (Layout)
    {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;  // Allow queue other than graphic
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;  // Allow queue other than graphic
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return VK_PIPELINE_STAGE_HOST_BIT;
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        default:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
}

}  // namespace

void Renderer::ImageLayoutBarrier(VkCommandBuffer Cmdbuffer, VkImage TargetImage,
                                  VkImageLayout OldImageLayout,
                                  VkImageLayout NewImageLayout,
                                  VkImageAspectFlags AspectMask)
{
    VkImageSubresourceRange SubresourceRange;
    SubresourceRange.aspectMask = AspectMask;
    SubresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    SubresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    SubresourceRange.baseMipLevel = 0;
    SubresourceRange.baseArrayLayer = 0;

    VkImageMemoryBarrier ImageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    ImageMemoryBarrier.oldLayout = OldImageLayout;
    ImageMemoryBarrier.newLayout = NewImageLayout;
    ImageMemoryBarrier.image = TargetImage;
    ImageMemoryBarrier.subresourceRange = SubresourceRange;
    ImageMemoryBarrier.srcAccessMask = AccessFlagsForImageLayout(OldImageLayout);
    ImageMemoryBarrier.dstAccessMask = AccessFlagsForImageLayout(NewImageLayout);
    ImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    VkPipelineStageFlags SrcStageMask = PipelineStageForLayout(OldImageLayout);
    VkPipelineStageFlags DestStageMask = PipelineStageForLayout(NewImageLayout);

    vkCmdPipelineBarrier(Cmdbuffer, SrcStageMask, DestStageMask, 0, 0, nullptr, 0,
                         nullptr, 1, &ImageMemoryBarrier);
}

Image Renderer::CreateTextureImage(const std::string& FileName)
{
    // VkImage& textureImage, VkDeviceMemory& textureImageMemory
    int TexWidth, TexHeight, TexChannels;

    stbi_set_flip_vertically_on_load(true);
    stbi_uc* Pixels =
        stbi_load(FileName.c_str(), &TexWidth, &TexHeight, &TexChannels, STBI_rgb_alpha);
    VkDeviceSize ImageSize = TexWidth * TexHeight * 4;

    if (!Pixels)
    {
        RTRT_FATAL("Failed to load texture %s: %s", FileName.c_str(),
                   stbi_failure_reason());
    }

    Buffer Staging = CreateBuffer(
        ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    memcpy(Staging.Map(ImageSize), Pixels, static_cast<size_t>(ImageSize));
    Staging.Unmap();

    stbi_image_free(Pixels);

    uint32_t MipLevels =
        static_cast<uint32_t>(std::floor(std::log2(std::max(TexWidth, TexHeight)))) + 1;

    Image MyImage =
        CreateImage(TexWidth, TexHeight, VK_FORMAT_R8G8B8A8_UNORM,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MipLevels);

    TransitionImageLayout(MyImage.GetHandle(), VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          MipLevels);

    CopyBufferToImage(Staging.GetHandle(), MyImage.GetHandle(),
                      static_cast<uint32_t>(TexWidth), static_cast<uint32_t>(TexHeight));

    Staging.Destroy();

    GenerateMipmaps(MyImage.GetHandle(), VK_FORMAT_R8G8B8A8_UNORM, TexWidth, TexHeight,
                    MipLevels);

    MyImage.CreateView(VK_FORMAT_R8G8B8A8_UNORM);
    MyImage.CreateSampler(*GpuDevice);
    MyImage.SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return MyImage;
}

namespace
{

// Blits mip level Level - 1 (MipWidth x MipHeight) down into level Level, then moves
// level Level - 1 to the shader-read layout.
void BlitMipLevel(VkCommandBuffer CmdBuffer, VkImage TargetImage,
                  VkImageMemoryBarrier& Barrier, uint32_t Level, int32_t MipWidth,
                  int32_t MipHeight)
{
    Barrier.subresourceRange.baseMipLevel = Level - 1;
    Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    Barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    Barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(CmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &Barrier);

    VkImageBlit Blit{};
    Blit.srcOffsets[0] = {0, 0, 0};
    Blit.srcOffsets[1] = {MipWidth, MipHeight, 1};
    Blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Blit.srcSubresource.mipLevel = Level - 1;
    Blit.srcSubresource.baseArrayLayer = 0;
    Blit.srcSubresource.layerCount = 1;
    Blit.dstOffsets[0] = {0, 0, 0};
    Blit.dstOffsets[1] = {MipWidth > 1 ? MipWidth / 2 : 1,
                          MipHeight > 1 ? MipHeight / 2 : 1, 1};
    Blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Blit.dstSubresource.mipLevel = Level;
    Blit.dstSubresource.baseArrayLayer = 0;
    Blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(CmdBuffer, TargetImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   TargetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Blit,
                   VK_FILTER_LINEAR);

    Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    Barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(CmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &Barrier);
}

}  // namespace

void Renderer::GenerateMipmaps(VkImage TargetImage, VkFormat ImageFormat,
                               int32_t TexWidth, int32_t TexHeight, uint32_t MipLevels)
{
    // Check if image format supports linear blitting
    VkFormatProperties FormatProperties;
    vkGetPhysicalDeviceFormatProperties(GpuDevice->GetPhysical(), ImageFormat,
                                        &FormatProperties);

    if (!(FormatProperties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        RTRT_FATAL("Texture image format does not support linear blitting");
    }

    VkCommandBuffer TempCmdBuffer = CreateTempCmdBuffer();

    VkImageMemoryBarrier Barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    Barrier.image = TargetImage;
    Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Barrier.subresourceRange.baseArrayLayer = 0;
    Barrier.subresourceRange.layerCount = 1;
    Barrier.subresourceRange.levelCount = 1;

    int32_t MipWidth = TexWidth;
    int32_t MipHeight = TexHeight;

    for (uint32_t I = 1; I < MipLevels; I++)
    {
        BlitMipLevel(TempCmdBuffer, TargetImage, Barrier, I, MipWidth, MipHeight);

        if (MipWidth > 1)
        {
            MipWidth /= 2;
        }
        if (MipHeight > 1)
        {
            MipHeight /= 2;
        }
    }

    // The last level was only written to, never blitted from.
    Barrier.subresourceRange.baseMipLevel = MipLevels - 1;
    Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    Barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(TempCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &Barrier);

    SubmitTempCmdBuffer(TempCmdBuffer);
}

Buffer Renderer::CreateStagedBuffer(VkCommandBuffer CmdBuf, VkDeviceSize Size,
                                    const void* Data, VkBufferUsageFlags Usage)
{
    Buffer Staging = CreateBuffer(
        Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    memcpy(Staging.Map(Size), Data, Size);
    Staging.Unmap();

    Buffer Bw = CreateBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | Usage,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CopyBuffer(Staging.GetHandle(), Bw.GetHandle(), Size);

    Staging.Destroy();

    return Bw;
}

Buffer Renderer::CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage,
                              VkMemoryPropertyFlags Properties)
{
    Buffer Result;
    Result.Create(*GpuDevice, Size, Usage, Properties);
    return Result;
}

void Renderer::CopyBuffer(VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize Size)
{
    VkCommandBuffer TempCmdBuffer = CreateTempCmdBuffer();

    VkBufferCopy CopyRegion{};
    CopyRegion.size = Size;
    vkCmdCopyBuffer(TempCmdBuffer, SrcBuffer, DstBuffer, 1, &CopyRegion);

    SubmitTempCmdBuffer(TempCmdBuffer);
}

void Renderer::CopyBufferToImage(VkBuffer SourceBuffer, VkImage TargetImage,
                                 uint32_t Width, uint32_t Height)
{
    VkCommandBuffer TempCmdBuffer = CreateTempCmdBuffer();

    VkBufferImageCopy Region{};
    Region.bufferOffset = 0;
    Region.bufferRowLength = 0;
    Region.bufferImageHeight = 0;
    Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Region.imageSubresource.mipLevel = 0;
    Region.imageSubresource.baseArrayLayer = 0;
    Region.imageSubresource.layerCount = 1;
    Region.imageOffset = {0, 0, 0};
    Region.imageExtent = {Width, Height, 1};

    vkCmdCopyBufferToImage(TempCmdBuffer, SourceBuffer, TargetImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

    SubmitTempCmdBuffer(TempCmdBuffer);
}

void Renderer::TransitionImageLayout(VkImage TargetImage, VkFormat Format,
                                     VkImageLayout OldLayout, VkImageLayout NewLayout,
                                     uint32_t MipLevels)
{
    VkCommandBuffer TempCmdBuffer = CreateTempCmdBuffer();

    VkImageMemoryBarrier Barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    Barrier.oldLayout = OldLayout;
    Barrier.newLayout = NewLayout;
    Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.image = TargetImage;
    Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Barrier.subresourceRange.baseMipLevel = 0;
    Barrier.subresourceRange.levelCount = MipLevels;
    Barrier.subresourceRange.baseArrayLayer = 0;
    Barrier.subresourceRange.layerCount = 1;
    Barrier.srcAccessMask = 0;
    Barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    VkPipelineStageFlags SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags DestinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdPipelineBarrier(TempCmdBuffer, SourceStage, DestinationStage, 0, 0, nullptr, 0,
                         nullptr, 1, &Barrier);

    SubmitTempCmdBuffer(TempCmdBuffer);
}

void Renderer::CreatePostDescriptor()
{
    DescriptorBuilder()
        .AddCombinedImageSampler(VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build(GpuDevice->GetHandle(), PostDesc);
    PostDesc.Set.Write(0, ScImageBuffer.Descriptor());
}

void Renderer::CreateScBuffer()
{
    ScImageBuffer = CreateBufferImage(MainSwapchain->GetExtent());

    VkCommandBuffer CmdBuf = CreateTempCmdBuffer();
    ImageLayoutBarrier(CmdBuf, ScImageBuffer.GetHandle(), VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_IMAGE_LAYOUT_GENERAL);
    SubmitTempCmdBuffer(CmdBuf);
}

Image Renderer::CreateBufferImage(VkExtent2D Size)
{
    uint32_t MipLevels = 1;

    Image MyImage =
        CreateImage(Size.width, Size.height, VK_FORMAT_R32G32B32A32_SFLOAT,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MipLevels);

    MyImage.CreateView(VK_FORMAT_R32G32B32A32_SFLOAT);
    MyImage.CreateSampler(*GpuDevice);
    MyImage.SetLayout(VK_IMAGE_LAYOUT_GENERAL);
    return MyImage;
}

// The scanline renderpass outputs to ScImageBuffer (as wrapped by
// ScanlineFramebuffer)
void Renderer::CreateScanlineRenderPass()
{
    VkAttachmentDescription ColorAttachment{};
    ColorAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentDescription DepthAttachment{};
    DepthAttachment.format = VK_FORMAT_X8_D24_UNORM_PACK32;
    DepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference ColorAttachmentRef{};
    ColorAttachmentRef.attachment = 0;
    ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference DepthAttachmentRef{};
    DepthAttachmentRef.attachment = 1;
    DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription Subpass{};
    Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    Subpass.colorAttachmentCount = 1;
    Subpass.pColorAttachments = &ColorAttachmentRef;
    Subpass.pDepthStencilAttachment = &DepthAttachmentRef;

    VkSubpassDependency Dependency{};
    Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependency.dstSubpass = 0;
    Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    Dependency.srcAccessMask = 0;
    Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> AttachmentsDsc = {ColorAttachment,
                                                             DepthAttachment};
    VkRenderPassCreateInfo RenderPassInfo{};
    RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassInfo.attachmentCount = static_cast<uint32_t>(AttachmentsDsc.size());
    RenderPassInfo.pAttachments = AttachmentsDsc.data();
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &Subpass;
    RenderPassInfo.dependencyCount = 1;
    RenderPassInfo.pDependencies = &Dependency;

    RTRT_CHECK_VK(vkCreateRenderPass(GpuDevice->GetHandle(), &RenderPassInfo, nullptr,
                                     &ScanlineRenderPass));

    CreateScanlineFramebuffer();
}

void Renderer::CreateScanlineFramebuffer()
{
    std::vector<VkImageView> Attachments = {ScImageBuffer.GetView(),
                                            DepthImage.GetView()};

    VkFramebufferCreateInfo Info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    Info.renderPass = ScanlineRenderPass;
    Info.attachmentCount = static_cast<uint32_t>(Attachments.size());
    Info.pAttachments = Attachments.data();
    Info.width = MainSwapchain->GetExtent().width;
    Info.height = MainSwapchain->GetExtent().height;
    Info.layers = 1;
    RTRT_CHECK_VK(vkCreateFramebuffer(GpuDevice->GetHandle(), &Info, nullptr,
                                      &ScanlineFramebuffer));
}

void Renderer::CreateScDescriptorSet()
{
    auto NbTxt = static_cast<uint32_t>(ObjTextures.size());

    // In ScBindings order: kMatrices, kObjDescs, kTextures
    DescriptorBuilder()
        .AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddStorageBuffer(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                          VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddCombinedImageSampler(
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR, NbTxt)
        .Build(GpuDevice->GetHandle(), ScDesc);

    ScDesc.Set.Write(ScBindings::kMatrices, MatrixBw.GetHandle());
    ScDesc.Set.Write(ScBindings::kObjDescs, ObjDescriptionBw.GetHandle());
    ScDesc.Set.Write(ScBindings::kTextures, ObjTextures);
}

void Renderer::CreateScPipeline()
{
    VkPushConstantRange PushConstantRanges = {
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
        sizeof(PushConstantRaster)};

    // Creating the Pipeline Layout
    VkPipelineLayoutCreateInfo CreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    CreateInfo.setLayoutCount = 1;
    CreateInfo.pSetLayouts = &ScDesc.Layout.GetHandle();
    CreateInfo.pushConstantRangeCount = 1;
    CreateInfo.pPushConstantRanges = &PushConstantRanges;
    RTRT_CHECK_VK(vkCreatePipelineLayout(GpuDevice->GetHandle(), &CreateInfo, nullptr,
                                         &ScanlinePipelineLayout));

    VkVertexInputBindingDescription BindingDescription{0, sizeof(Vertex),
                                                       VK_VERTEX_INPUT_RATE_VERTEX};

    std::vector<VkVertexInputAttributeDescription> AttributeDescriptions{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, Pos))},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, Nrm))},
        {2, 0, VK_FORMAT_R32G32_SFLOAT,
         static_cast<uint32_t>(offsetof(Vertex, TexCoord))}};

    VkPipelineVertexInputStateCreateInfo VertexInputInfo{};
    VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputInfo.vertexBindingDescriptionCount = 1;
    VertexInputInfo.pVertexBindingDescriptions = &BindingDescription;
    VertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(AttributeDescriptions.size());
    VertexInputInfo.pVertexAttributeDescriptions = AttributeDescriptions.data();

    const GraphicsShaders Shaders{"spv/scanline.vert.spv", "spv/scanline.vert.slang.spv",
                                  "spv/scanline.frag.spv", "spv/scanline.frag.slang.spv"};
    ScanlinePipeline = CreateGraphicsPipeline(Shaders, VertexInputInfo,
                                              ScanlinePipelineLayout, ScanlineRenderPass);
}

// Create a Vulkan buffer to hold the camera matrices, products and inverses.
// Will be included in a descriptor set for use in shaders.
void Renderer::CreateMatrixBuffer()
{
    MatrixBw = CreateBuffer(
        sizeof(MatrixUniforms),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

// Create a Vulkan buffer containing pointers to all object buffers
// (vertex, triangle indices, materials, and material indices. Will be
// included in a descriptor set for use in shaders.
void Renderer::CreateObjDescriptionBuffer()
{
    VkCommandBuffer CmdBuf = CreateTempCmdBuffer();
    ObjDescriptionBw =
        CreateStagedBuffer(CmdBuf, ObjDescList, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    SubmitTempCmdBuffer(CmdBuf);
}

void Renderer::Rasterize()
{
    VkDeviceSize Offset{0};

    std::array<VkClearValue, 2> ClearValues{};
    ClearValues[0].color = {{0, 0, 0, 1}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo PassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    PassBeginInfo.clearValueCount = 2;
    PassBeginInfo.pClearValues = ClearValues.data();
    PassBeginInfo.renderPass = ScanlineRenderPass;
    PassBeginInfo.framebuffer = ScanlineFramebuffer;
    PassBeginInfo.renderArea = {{0, 0}, MainSwapchain->GetExtent()};
    vkCmdBeginRenderPass(FrameCmd.GetHandle(), &PassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(FrameCmd.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                      ScanlinePipeline);
    vkCmdBindDescriptorSets(FrameCmd.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            ScanlinePipelineLayout, 0, 1, &ScDesc.Set.GetHandle(), 0,
                            nullptr);

    for (const ObjInst& Inst : ObjInstList)
    {
        auto& Object = ObjDataList[Inst.ObjIndex];

        // Information pushed at each draw call
        PushConstantRaster PcRaster{
            Inst.Transform,      // Object's instance transform.
            {0.5f, 2.5f, 3.0f},  // light position;  Should not be hard-coded here!
            Inst.ObjIndex,       // instance Id
            1.0f                 // light intensity;  Should not be hard-coded here!
        };

        PcRaster.ObjIndex = Inst.ObjIndex;  // Telling which object is drawn
        PcRaster.ModelMatrix = Inst.Transform;

        vkCmdPushConstants(FrameCmd.GetHandle(), ScanlinePipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(PushConstantRaster), &PcRaster);
        const VkBuffer VertexBuffer = Object.VertexBuffer.GetHandle();
        vkCmdBindVertexBuffers(FrameCmd.GetHandle(), 0, 1, &VertexBuffer, &Offset);
        vkCmdBindIndexBuffer(FrameCmd.GetHandle(), Object.IndexBuffer.GetHandle(), 0,
                             VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(FrameCmd.GetHandle(), Object.NbIndices, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(FrameCmd.GetHandle());
}

void Renderer::UpdateCameraBuffer()
{
    // Prepare new UBO contents on host.
    const float AspectRatio = MainSwapchain->GetExtent().width /
                              static_cast<float>(MainSwapchain->GetExtent().height);
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

}  // namespace rtrt
