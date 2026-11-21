#include "renderer.h"

#include <array>
#include <vector>

#include <glm/glm.hpp>

#include "shaders/shared_structs.h"
#include "vk_check.h"

namespace rtrt
{

void Renderer::CreateScBuffer()
{
    ScImageBuffer = CreateBufferImage(MainSwapchain->GetExtent());

    VkCommandBuffer CmdBuf = MainCmdPool.BeginSingleUse();
    CmdImageBarrier(CmdBuf, ScImageBuffer.GetHandle(), VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_GENERAL);
    MainCmdPool.EndSingleUse(CmdBuf);
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
