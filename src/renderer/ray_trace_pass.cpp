#include "renderer/ray_trace_pass.h"

#include <cstdint>
#include <cstdlib>
#include <vector>

#include <glm/glm.hpp>

#include "renderer/render_image.h"

namespace rtrt
{

void RayTracePass::Create(const PassContext& Context, VkAccelerationStructureKHR Tlas,
                          VkDescriptorSetLayout SceneLayout)
{
    CreateImages(Context);

    // TLAS, then the output image and the six history images
    DescriptorBuilder()
        .AddAccelerationStructure(VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                  VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .AddStorageImage(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddStorageImage(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddStorageImage(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddStorageImage(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddStorageImage(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .AddStorageImage(VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .Build(Context.Gpu->GetHandle(), Desc);

    Desc.Set.Write(0, Tlas);
    WriteImageDescriptors();

    CreatePipeline(Context, SceneLayout);
    // One raygen, one miss and one hit group
    Sbt.Create(*Context.Gpu, *Context.Pool, TracePipeline.GetHandle(), 1, 1);
}

void RayTracePass::ReleaseSized()
{
    ColCurr.Destroy();
    ColPrev.Destroy();
    NdCurr.Destroy();
    NdPrev.Destroy();
    KdCurr.Destroy();
    KdPrev.Destroy();
}

void RayTracePass::OnResize(const PassContext& Context)
{
    CreateImages(Context);
    WriteImageDescriptors();
}

void RayTracePass::RebuildPipeline(const PassContext& Context,
                                   VkDescriptorSetLayout SceneLayout)
{
    // The SBT holds group handles of the old pipeline, so it is rebuilt with it
    Sbt.Destroy();
    TracePipeline.Destroy();
    TraceLayout.Destroy();
    CreatePipeline(Context, SceneLayout);
    Sbt.Create(*Context.Gpu, *Context.Pool, TracePipeline.GetHandle(), 1, 1);
}

void RayTracePass::Destroy()
{
    ReleaseSized();
    Desc.Destroy();
    TraceLayout.Destroy();
    TracePipeline.Destroy();
    Sbt.Destroy();
}

void RayTracePass::Record(VkCommandBuffer Cmd, const PassContext& Context,
                          VkDescriptorSet SceneSet, const Image& Output, int ViewMode,
                          bool Clear)
{
    // The push constants for the ray tracing pipeline.
    // These are temporary -- used only for ray casting step.
    Constants.AlignmentTest = 1234;
    Constants.TempLightPos = vec4(0.5f, 2.5f, 3.0f, 0.0);
    Constants.TempLightInt = vec4(2.5, 2.5, 2.5, 0.0);
    Constants.TempAmbient = vec4(0.2f);

    Constants.FrameSeed = rand() % 32768;
    Constants.Clear = Clear;

    // Bind the ray tracing pipeline
    vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                      TracePipeline.GetHandle());

    // Bind the descriptor sets (the ray tracing specific one, and the
    // full model descriptor)
    std::vector<VkDescriptorSet> DescSets{Desc.Set.GetHandle(), SceneSet};
    vkCmdBindDescriptorSets(
        Cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, TraceLayout.GetHandle(), 0,
        static_cast<uint32_t>(DescSets.size()), DescSets.data(), 0, nullptr);

    // Push the push constants
    vkCmdPushConstants(Cmd, TraceLayout.GetHandle(),
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR,
                       0, sizeof(PushConstantRay), &Constants);

    // This dispatches the ray generation shader for each pixel on screen.
    Sbt.CmdTraceRays(Cmd, Context.Chain->GetExtent().width,
                     Context.Chain->GetExtent().height);

    // Copy the ray tracer output image to the scanline output image
    // -- because we already have the operations needed to display
    // that image on the screen.  The debug views show a G-buffer image instead.
    if (ViewMode == kViewAlbedo)
    {
        CmdCopyImage(Cmd, KdCurr, Output, Context.Chain->GetExtent());
    }
    else if (ViewMode == kViewNormal || ViewMode == kViewDepth)
    {
        CmdCopyImage(Cmd, NdCurr, Output, Context.Chain->GetExtent());
    }
    else
    {
        CmdCopyImage(Cmd, ColCurr, Output, Context.Chain->GetExtent());
    }

    // Save current frame buffers as history for the next frame
    CmdCopyImage(Cmd, ColCurr, ColPrev, Context.Chain->GetExtent());
    CmdCopyImage(Cmd, KdCurr, KdPrev, Context.Chain->GetExtent());
    CmdCopyImage(Cmd, NdCurr, NdPrev, Context.Chain->GetExtent());
}

void RayTracePass::CreateImages(const PassContext& Context)
{
    ColCurr = CreateRenderImage(*Context.Gpu, Context.Chain->GetExtent());
    ColCurr.TransitionLayout(*Context.Pool, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL);

    ColPrev = CreateRenderImage(*Context.Gpu, Context.Chain->GetExtent());
    ColPrev.TransitionLayout(*Context.Pool, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL);

    NdCurr = CreateRenderImage(*Context.Gpu, Context.Chain->GetExtent());
    NdCurr.TransitionLayout(*Context.Pool, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_GENERAL);

    NdPrev = CreateRenderImage(*Context.Gpu, Context.Chain->GetExtent());
    NdPrev.TransitionLayout(*Context.Pool, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_GENERAL);

    KdCurr = CreateRenderImage(*Context.Gpu, Context.Chain->GetExtent());
    KdCurr.TransitionLayout(*Context.Pool, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_GENERAL);

    KdPrev = CreateRenderImage(*Context.Gpu, Context.Chain->GetExtent());
    KdPrev.TransitionLayout(*Context.Pool, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_GENERAL);
}

void RayTracePass::WriteImageDescriptors() const
{
    Desc.Set.Write(1, ColCurr.Descriptor());
    Desc.Set.Write(2, ColPrev.Descriptor());
    Desc.Set.Write(3, NdCurr.Descriptor());
    Desc.Set.Write(4, NdPrev.Descriptor());
    Desc.Set.Write(5, KdCurr.Descriptor());
    Desc.Set.Write(6, KdPrev.Descriptor());
}

void RayTracePass::CreatePipeline(const PassContext& Context,
                                  VkDescriptorSetLayout SceneLayout)
{
    std::vector<VkPipelineShaderStageCreateInfo> Stages{};
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> Groups{};
    // The pipeline keeps compiled copies, so the modules are freed when this returns.
    std::vector<Shader> Modules;
    CollectShaders(Context, Modules, Stages, Groups);

    // Push constant: we want to be able to update constants used by the shaders
    VkPushConstantRange PushConstant{VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                         VK_SHADER_STAGE_MISS_BIT_KHR,
                                     0, sizeof(PushConstantRay)};

    // Descriptor sets: one specific to ray tracing, and one shared with the rasterization
    // pipeline
    TraceLayout.Create(Context.Gpu->GetHandle(), {Desc.Layout.GetHandle(), SceneLayout},
                       {PushConstant});

    // Assemble the shader stages and recursion depth info into the ray tracing pipeline
    VkRayTracingPipelineCreateInfoKHR RayPipelineInfo{
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    // Stages are shaders
    RayPipelineInfo.stageCount = static_cast<uint32_t>(Stages.size());
    RayPipelineInfo.pStages = Stages.data();

    RayPipelineInfo.groupCount = static_cast<uint32_t>(Groups.size());
    RayPipelineInfo.pGroups = Groups.data();

    RayPipelineInfo.maxPipelineRayRecursionDepth = 10;  // Ray depth
    RayPipelineInfo.layout = TraceLayout.GetHandle();

    TracePipeline.CreateRayTracing(Context.Gpu->GetHandle(), RayPipelineInfo);
}

void RayTracePass::CollectShaders(
    const PassContext& Context, std::vector<Shader>& Modules,
    std::vector<VkPipelineShaderStageCreateInfo>& Stages,
    std::vector<VkRayTracingShaderGroupCreateInfoKHR>& Groups) const
{
    // Raygen and miss shaders get their own groups. Hit shaders can group with any-hit
    // and intersection shaders -- of which we have none -- so the hit shader gets its
    // own group too.
    VkPipelineShaderStageCreateInfo Stage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    Stage.pName = "main";  // All the same entry point

    VkRayTracingShaderGroupCreateInfoKHR Group{
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
    Group.anyHitShader = VK_SHADER_UNUSED_KHR;
    Group.closestHitShader = VK_SHADER_UNUSED_KHR;
    Group.generalShader = VK_SHADER_UNUSED_KHR;
    Group.intersectionShader = VK_SHADER_UNUSED_KHR;

    // Raygen shader stage and group appended to stages and groups lists
    Modules.emplace_back().Load(Context.Gpu->GetHandle(), "spv/raytrace.rgen.spv",
                                "spv/raytrace.rgen.slang.spv", Context.UseSlang);
    Stage.module = Modules.back().GetHandle();
    Stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    Stages.push_back(Stage);

    Group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    // Index of raygen shader
    Group.generalShader = static_cast<uint32_t>(Stages.size() - 1);
    Groups.push_back(Group);
    Group.generalShader = VK_SHADER_UNUSED_KHR;

    // Miss shader stage and group appended to stages and groups lists
    Modules.emplace_back().Load(Context.Gpu->GetHandle(), "spv/raytrace.rmiss.spv",
                                "spv/raytrace.rmiss.slang.spv", Context.UseSlang);
    Stage.module = Modules.back().GetHandle();
    Stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    Stages.push_back(Stage);

    Group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    // Index of miss shader
    Group.generalShader = static_cast<uint32_t>(Stages.size() - 1);
    Groups.push_back(Group);
    Group.generalShader = VK_SHADER_UNUSED_KHR;

    // Closest hit shader stage and group appended to stages and groups lists
    Modules.emplace_back().Load(Context.Gpu->GetHandle(), "spv/raytrace.rchit.spv",
                                "spv/raytrace.rchit.slang.spv", Context.UseSlang);
    Stage.module = Modules.back().GetHandle();
    Stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    Stages.push_back(Stage);

    Group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    // Index of hit shader
    Group.closestHitShader = static_cast<uint32_t>(Stages.size() - 1);
    Groups.push_back(Group);
}

}  // namespace rtrt
