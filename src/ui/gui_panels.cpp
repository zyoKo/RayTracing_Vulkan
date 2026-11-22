#include "ui/gui.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <future>

#include <glm/glm.hpp>
#include <imgui.h>

#include "core/shader_check.h"
#include "shaders/shared_structs.h"
#include "ui/property_grid.h"
#include "ui/theme.h"

namespace rtrt
{

namespace
{

constexpr ImGuiTreeNodeFlags kLeafFlags = ImGuiTreeNodeFlags_Leaf |
                                          ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                          ImGuiTreeNodeFlags_SpanAvailWidth;
// UTF-8 degree sign
constexpr const char* kDegreeFormat = "%.1f\xC2\xB0";

ImVec4 ToColor(const glm::vec3& Value)
{
    return ImVec4(Value.x, Value.y, Value.z, 1.0f);
}

// The inspector's title: a bold name over a dim kind.
void InspectorHeader(ImFont* Bold, const char* Name, const char* Kind)
{
    ImGui::PushFont(Bold);
    ImGui::TextUnformatted(Name);
    ImGui::PopFont();
    ImGui::TextDisabled("%s", Kind);
    ImGui::Separator();
}

}  // namespace

void Gui::DrawOutliner(Renderer& VK)
{
    if (!ImGui::Begin("Outliner", &ShowOutliner))
    {
        ImGui::End();
        return;
    }
    const Scene& World = VK.GetScene();

    ImGui::SetNextItemWidth(-FLT_MIN);
    MaterialFilter.Draw("##Filter", -FLT_MIN);
    if (!MaterialFilter.IsActive() && !ImGui::IsItemActive())
    {
        const ImVec2 Min = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(Min.x + ImGui::GetStyle().FramePadding.x,
                   Min.y + ImGui::GetStyle().FramePadding.y),
            ImGui::GetColorU32(kTextDim), "Filter materials");
    }
    ImGui::Spacing();

    if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen |
                                       ImGuiTreeNodeFlags_SpanAvailWidth))
    {
        ImGui::TreeNodeEx("Camera", kLeafFlags | (Selected == Selection::kCamera
                                                      ? ImGuiTreeNodeFlags_Selected
                                                      : 0));
        if (ImGui::IsItemClicked())
        {
            Selected = Selection::kCamera;
        }

        for (uint32_t MeshIndex = 0; MeshIndex < World.GetMeshes().size(); MeshIndex++)
        {
            const Mesh& Model = World.GetMeshes()[MeshIndex];
            const bool ModelSelected =
                Selected == Selection::kModel && SelectedMesh == MeshIndex;
            ImGui::PushID(static_cast<int>(MeshIndex));
            const bool Open = ImGui::TreeNodeEx(
                "##Model",
                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow |
                    ImGuiTreeNodeFlags_SpanAvailWidth |
                    (ModelSelected ? ImGuiTreeNodeFlags_Selected : 0),
                "%s", Model.Name.c_str());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                Selected = Selection::kModel;
                SelectedMesh = MeshIndex;
            }
            if (Open)
            {
                char Label[64];
                snprintf(Label, sizeof(Label), "Materials (%zu)", Model.Materials.size());
                if (ImGui::TreeNodeEx(Label, ImGuiTreeNodeFlags_DefaultOpen |
                                                 ImGuiTreeNodeFlags_SpanAvailWidth))
                {
                    for (uint32_t I = 0; I < Model.Materials.size(); I++)
                    {
                        const char* Name = Model.MaterialNames[I].c_str();
                        if (!MaterialFilter.PassFilter(Name))
                        {
                            continue;
                        }
                        ImGui::PushID(static_cast<int>(I));
                        ImGui::ColorButton("##Swatch", ToColor(Model.Materials[I].Diffuse),
                                           ImGuiColorEditFlags_NoTooltip,
                                           ImVec2(10.0f, 10.0f));
                        ImGui::SameLine(0.0f, 6.0f);
                        const bool IsSelected = Selected == Selection::kMaterial &&
                                                SelectedMesh == MeshIndex &&
                                                SelectedMaterial == I;
                        if (ImGui::Selectable(Name, IsSelected))
                        {
                            Selected = Selection::kMaterial;
                            SelectedMesh = MeshIndex;
                            SelectedMaterial = I;
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }

                snprintf(Label, sizeof(Label), "Textures (%zu)",
                         World.GetTextureNames().size());
                if (ImGui::TreeNodeEx(Label, ImGuiTreeNodeFlags_SpanAvailWidth))
                {
                    for (const std::string& Name : World.GetTextureNames())
                    {
                        ImGui::TreeNodeEx(Name.c_str(), kLeafFlags);
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    ImGui::End();
}

void Gui::DrawInspector(Renderer& VK, Camera& View, CameraController& Controller)
{
    if (!ImGui::Begin("Inspector", &ShowInspector))
    {
        ImGui::End();
        return;
    }
    switch (Selected)
    {
        case Selection::kCamera:
            DrawCameraInspector(View, Controller);
            break;
        case Selection::kModel:
            DrawModelInspector(VK);
            break;
        case Selection::kMaterial:
            DrawMaterialInspector(VK);
            break;
        case Selection::kNone:
            ImGui::TextDisabled("Select an item in the Outliner.");
            break;
    }
    ImGui::End();
}

void Gui::DrawCameraInspector(Camera& View, CameraController& Controller)
{
    InspectorHeader(Fonts.Bold, "Camera", "Perspective fly camera");

    if (Section(Fonts.Bold, "Transform") && BeginProperties("CameraTransform"))
    {
        glm::vec3 Eye = View.GetEye();
        PropertyRow("Position", "World-space eye position, in metres");
        if (ImGui::DragFloat3("##Position", &Eye.x, 0.01f, 0.0f, 0.0f, "%.2f"))
        {
            View.SetEye(Eye);
        }
        float Angles[2] = {View.GetSpin(), View.GetTilt()};
        PropertyRow("Yaw / pitch", "Degrees");
        if (ImGui::DragFloat2("##Angles", Angles, 0.2f, 0.0f, 0.0f, kDegreeFormat))
        {
            View.SetAngles(Angles[0], Angles[1]);
        }
        EndProperties();
    }

    if (Section(Fonts.Bold, "Lens") && BeginProperties("CameraLens"))
    {
        float Fov = View.GetFovY();
        PropertyRow("Vertical FOV");
        if (ImGui::SliderFloat("##Fov", &Fov, 20.0f, 110.0f, kDegreeFormat))
        {
            View.SetFovY(Fov);
        }
        EndProperties();
    }

    if (Section(Fonts.Bold, "Navigation") && BeginProperties("CameraNavigation"))
    {
        float MoveSpeed = Controller.GetMoveSpeed();
        PropertyRow("Move speed", "Also the mouse wheel over the viewport");
        if (ImGui::SliderFloat("##Speed", &MoveSpeed, 0.05f, 20.0f, "%.2f m/s",
                               ImGuiSliderFlags_Logarithmic))
        {
            Controller.SetMoveSpeed(MoveSpeed);
        }
        EndProperties();
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset Camera", ImVec2(-FLT_MIN, 0.0f)))
    {
        View.Reset();
    }
}

void Gui::DrawModelInspector(Renderer& VK)
{
    const Scene& World = VK.GetScene();
    if (SelectedMesh >= World.GetMeshes().size())
    {
        return;
    }
    const Mesh& Model = World.GetMeshes()[SelectedMesh];
    InspectorHeader(Fonts.Bold, Model.Name.c_str(), "Triangle mesh");

    if (Section(Fonts.Bold, "Geometry") && BeginProperties("ModelGeometry"))
    {
        PropertyText("Vertices", "%u", Model.NbVertices);
        PropertyText("Triangles", "%u", Model.NbIndices / 3);
        PropertyText("Materials", "%zu", Model.Materials.size());
        PropertyText("Textures", "%zu", World.GetTextures().size());
        uint32_t Instances = 0;
        for (const MeshInstance& Instance : World.GetInstances())
        {
            Instances += Instance.ObjIndex == SelectedMesh ? 1 : 0;
        }
        PropertyText("Instances", "%u", Instances);
        EndProperties();
    }

    if (Section(Fonts.Bold, "Ray tracing") && BeginProperties("ModelRayTracing"))
    {
        PropertyText("Acceleration", "1 BLAS, opaque");
        PropertyText("Build flags", "Prefer fast trace");
        EndProperties();
    }
}

void Gui::DrawMaterialInspector(Renderer& VK)
{
    Scene& World = VK.GetScene();
    if (SelectedMesh >= World.GetMeshes().size() ||
        SelectedMaterial >= World.GetMeshes()[SelectedMesh].Materials.size())
    {
        return;
    }
    const Mesh& Model = World.GetMeshes()[SelectedMesh];
    Material Value = Model.Materials[SelectedMaterial];
    InspectorHeader(Fonts.Bold, Model.MaterialNames[SelectedMaterial].c_str(),
                    "GGX material");

    bool Changed = false;
    if (Section(Fonts.Bold, "Surface") && BeginProperties("MaterialSurface"))
    {
        PropertyRow("Base color", Value.TextureId >= 0
                                      ? "Multiplied with the base color texture"
                                      : "Diffuse albedo");
        Changed |= ImGui::ColorEdit3("##Diffuse", &Value.Diffuse.x,
                                     ImGuiColorEditFlags_Float);
        PropertyRow("Specular", "Specular reflectance at normal incidence");
        Changed |= ImGui::ColorEdit3("##Specular", &Value.Specular.x,
                                     ImGuiColorEditFlags_Float);
        PropertyRow("Roughness", "GGX roughness: 0 is a mirror, 1 is fully rough");
        Changed |= ImGui::SliderFloat("##Roughness", &Value.Shininess, 0.0f, 1.0f, "%.3f");
        EndProperties();
    }

    if (Section(Fonts.Bold, "Emission") && BeginProperties("MaterialEmission"))
    {
        PropertyRow("Radiance", "Emitted radiance; values above 1 are allowed");
        Changed |= ImGui::ColorEdit3("##Emission", &Value.Emission.x,
                                     ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        EndProperties();
    }

    if (Section(Fonts.Bold, "Info") && BeginProperties("MaterialInfo"))
    {
        const bool HasTexture =
            Value.TextureId >= 0 &&
            static_cast<size_t>(Value.TextureId) < World.GetTextureNames().size();
        PropertyText("Texture", "%s",
                     HasTexture ? World.GetTextureNames()[Value.TextureId].c_str()
                                : "None");
        PropertyText("Triangles", "%u", Model.MaterialTriangles[SelectedMaterial]);
        PropertyText("Index", "%u", SelectedMaterial);
        EndProperties();
    }

    if (Changed)
    {
        World.SetMaterial(SelectedMesh, SelectedMaterial, Value);
    }
}

void Gui::DrawRenderSettings(Renderer& VK)
{
    if (!ImGui::Begin("Render Settings", &ShowRenderSettings))
    {
        ImGui::End();
        return;
    }

    // Anything that changes what a path returns makes the existing history wrong.
    bool ResetHistory = DrawIntegratorSettings(VK);
    ResetHistory |= DrawAccumulationSettings(VK);
    DrawDenoiserSettings(VK);
    DrawDisplaySettings(VK);
    DrawShaderSettings(VK);

    ImGui::Spacing();
    if (ImGui::Button("Restore Defaults", ImVec2(-FLT_MIN, 0.0f)))
    {
        VK.SetDefaultSettings();
        Log(LogLevel::kInfo, "Render settings restored to defaults");
    }

    if (ResetHistory)
    {
        VK.ResetRtAccumulation();
    }
    ImGui::End();
}

bool Gui::DrawIntegratorSettings(Renderer& VK)
{
    if (!Section(Fonts.Bold, "Integrator") || !BeginProperties("Integrator"))
    {
        return false;
    }
    RenderSettings& Settings = VK.GetSettings();
    PushConstantRay& Ray = VK.GetRayConstants();
    bool ResetHistory = false;

    int Mode = Settings.UseRaytracer ? 0 : 1;
    PropertyRow("Renderer");
    if (ImGui::Combo("##Renderer", &Mode, "Path tracer\0Rasterizer\0"))
    {
        ResetHistory |= Mode == 0 && !Settings.UseRaytracer;
        Settings.UseRaytracer = Mode == 0;
    }

    ImGui::BeginDisabled(!Settings.UseRaytracer);
    PropertyRow("Specular BRDF", "Off: Lambertian only");
    ResetHistory |= ImGui::Checkbox("##Specular", &Ray.FullBrdf);
    PropertyRow("Max path length", "Bounces before a path is cut off");
    ResetHistory |= ImGui::SliderInt("##Depth", &Ray.Depth, 1, 32, "%d bounces");
    PropertyRow("Survival chance", "Russian roulette probability, tested per bounce");
    ResetHistory |= ImGui::SliderFloat("##Rr", &Ray.Rr, 0.1f, 0.95f, "%.2f");
    PropertyRow("Emission scale", "Multiplier on every emitter");
    ResetHistory |= ImGui::SliderFloat("##Emission", &Ray.EmissionScale, 0.0f, 5.0f, "%.2f");
    ImGui::EndDisabled();
    EndProperties();
    return ResetHistory;
}

bool Gui::DrawAccumulationSettings(Renderer& VK)
{
    if (!Section(Fonts.Bold, "Accumulation") || !BeginProperties("Accumulation"))
    {
        return false;
    }
    PushConstantRay& Ray = VK.GetRayConstants();
    bool ResetHistory = false;

    PropertyRow("Temporal history", "Reproject and blend earlier frames");
    ImGui::Checkbox("##UseHistory", &Ray.UseHistory);
    ImGui::BeginDisabled(!Ray.UseHistory);
    PropertyRow("Normal threshold", "Minimum normal agreement to reuse a sample");
    ImGui::SliderFloat("##NormalThreshold", &Ray.NormalThreshold, 0.0f, 1.0f, "%.2f");
    PropertyRow("Depth threshold", "Maximum relative depth change to reuse a sample");
    ImGui::SliderFloat("##DepthThreshold", &Ray.DepthThreshold, 0.001f, 0.2f, "%.3f");
    PropertyRow("Sample limit", "Stop accumulating at this many samples; 0 is unlimited");
    ImGui::SliderFloat("##MaxSamples", &Ray.MaxSamples, 0.0f, 4096.0f,
                       Ray.MaxSamples > 0.0f ? "%.0f spp" : "Unlimited",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::EndDisabled();

    PropertyRow("Progress");
    const int Frames = VK.GetFrameCount();
    char Overlay[48];
    if (Ray.MaxSamples > 0.0f)
    {
        snprintf(Overlay, sizeof(Overlay), "%d / %.0f spp", Frames, Ray.MaxSamples);
        ImGui::ProgressBar(std::min(static_cast<float>(Frames) / Ray.MaxSamples, 1.0f),
                           ImVec2(-FLT_MIN, 0.0f), Overlay);
    }
    else
    {
        ImGui::Text("%d spp", Frames);
    }
    EndProperties();

    if (ImGui::Button("Reset Accumulation", ImVec2(-FLT_MIN, 0.0f)))
    {
        ResetHistory = true;
        Log(LogLevel::kInfo, "Accumulation reset");
    }
    return ResetHistory;
}

void Gui::DrawDenoiserSettings(Renderer& VK)
{
    if (!Section(Fonts.Bold, "Denoiser") || !BeginProperties("Denoiser"))
    {
        return;
    }
    RenderSettings& Settings = VK.GetSettings();
    PushConstantDenoise& Denoise = VK.GetDenoiseConstants();

    PropertyRow("Enabled", "Edge-avoiding A-Trous wavelet filter");
    ImGui::Checkbox("##Enabled", &Settings.DenoiseEnabled);
    ImGui::BeginDisabled(!Settings.DenoiseEnabled);
    PropertyRow("Iterations", "Each pass doubles the filter step");
    ImGui::SliderInt("##Iterations", &Settings.NumAtrousIterations, 0, 8);
    PropertyRow("Normal weight");
    ImGui::SliderFloat("##Normal", &Denoise.NormFactor, 0.0f, 0.1f, "%.4f",
                       ImGuiSliderFlags_Logarithmic);
    PropertyRow("Depth weight");
    ImGui::SliderFloat("##Depth", &Denoise.DepthFactor, 0.0f, 0.1f, "%.4f",
                       ImGuiSliderFlags_Logarithmic);
    PropertyRow("Albedo weight");
    ImGui::SliderFloat("##Albedo", &Denoise.AlbedoFactor, 0.0f, 1.0f, "%.3f",
                       ImGuiSliderFlags_Logarithmic);
    PropertyRow("Color weight", "Halved on every pass");
    ImGui::SliderFloat("##Color", &Denoise.ColorFactor, 0.0f, 2.0f, "%.3f",
                       ImGuiSliderFlags_Logarithmic);
    bool ClampFireflies = Denoise.ClampFireflies != 0;
    PropertyRow("Clamp fireflies", "Clamp outlier pixels before filtering");
    if (ImGui::Checkbox("##Clamp", &ClampFireflies))
    {
        Denoise.ClampFireflies = ClampFireflies ? 1 : 0;
    }
    ImGui::EndDisabled();
    EndProperties();
}

void Gui::DrawDisplaySettings(Renderer& VK)
{
    if (!Section(Fonts.Bold, "Display") || !BeginProperties("Display"))
    {
        return;
    }
    RenderSettings& Settings = VK.GetSettings();
    PropertyRow("View mode", "Ray tracer only: the final image or a G-buffer channel");
    ImGui::BeginDisabled(!Settings.UseRaytracer);
    ImGui::Combo("##ViewMode", &Settings.ViewMode, kViewModeNames, kViewModeCount);
    ImGui::EndDisabled();
    PropertyRow("Exposure");
    ImGui::SliderFloat("##Exposure", &Settings.Exposure, 0.1f, 4.0f, "%.2f");
    PropertyRow("Gamma");
    ImGui::SliderFloat("##Gamma", &Settings.Gamma, 1.0f, 3.0f, "%.2f");
    EndProperties();
}

void Gui::DrawShaderSettings(Renderer& VK)
{
    if (!Section(Fonts.Bold, "Shaders", false))
    {
        return;
    }

    // A finished build switches to GLSL, or reloads it when it's already in use
    if (GlslBuild.valid() &&
        GlslBuild.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        GlslBuildFailed = !GlslBuild.get() || !GlslShadersBuilt();
        if (GlslBuildFailed)
        {
            Log(LogLevel::kError, "GLSL shader build failed, see the terminal");
        }
        else
        {
            Log(LogLevel::kInfo, "GLSL shaders built");
            WantGlsl = false;
            if (VK.GetUseSlang())
            {
                VK.SetShaderLanguage(false);
            }
            else
            {
                VK.ReloadShaders();
            }
        }
    }

    const bool Building = GlslBuild.valid();
    const bool GlslBuilt = GlslShadersBuilt();
    if (BeginProperties("Shaders"))
    {
        int Language = VK.GetUseSlang() && !WantGlsl ? 0 : 1;
        PropertyRow("Source", "Slang SPIR-V, or SPIR-V built from Slang's generated GLSL");
        if (ImGui::Combo("##Language", &Language, "Slang\0GLSL (generated)\0"))
        {
            if (Language == 0)
            {
                WantGlsl = false;
                VK.SetShaderLanguage(true);
            }
            else if (GlslBuilt)
            {
                VK.SetShaderLanguage(false);
            }
            else
            {
                WantGlsl = true;
            }
        }
        PropertyRow("GLSL build");
        if (Building)
        {
            ImGui::TextColored(kWarn, "Building...");
        }
        else if (GlslBuildFailed)
        {
            ImGui::TextColored(kError, "Failed, see the terminal");
        }
        else
        {
            ImGui::TextDisabled("%s", GlslBuilt ? "Up to date" : "Not built");
        }
        EndProperties();
    }

    if (!Building && (WantGlsl || !VK.GetUseSlang()))
    {
        if (ImGui::Button(GlslBuilt ? "Rebuild GLSL Shaders" : "Build GLSL Shaders",
                          ImVec2(-FLT_MIN, 0.0f)))
        {
            GlslBuildFailed = false;
            Log(LogLevel::kInfo, "Building GLSL shaders");
            GlslBuild = std::async(std::launch::async, [] { return CompileShaders(true); });
        }
    }
}

}  // namespace rtrt
