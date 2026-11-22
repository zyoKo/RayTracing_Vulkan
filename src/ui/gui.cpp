#include "ui/gui.h"

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "shaders/shared_structs.h"
#include "ui/property_grid.h"
#include "ui/theme.h"
#include "vk_check.h"

namespace rtrt
{

namespace
{

constexpr uint32_t kUiTextureSets = 16;
constexpr float kOverlayMargin = 8.0f;
constexpr double kResizeLogDelay = 0.5;
constexpr double kStatusMessageSeconds = 6.0;

// "screenshots/rtrt_20261121_190455.png", from the local time.
std::string ScreenshotPath()
{
    const std::time_t Now = std::time(nullptr);
    std::tm Local{};
#ifdef _WIN32
    localtime_s(&Local, &Now);
#else
    localtime_r(&Now, &Local);
#endif
    char Name[64];
    std::strftime(Name, sizeof(Name), "screenshots/rtrt_%Y%m%d_%H%M%S.png", &Local);
    return Name;
}

ImU32 OverlayBg()
{
    return IM_COL32(14, 15, 16, 215);
}

ImTextureID ToTextureId(VkDescriptorSet Set)
{
    return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(Set));
}

}  // namespace

void Gui::Init(const Window& Target, const Context& Instance, const Device& Gpu,
               VkRenderPass Pass, uint32_t ImageCount, Renderer& VK)
{
    DeviceHandle = Gpu.GetHandle();
    StartTime = glfwGetTime();

    VkPhysicalDeviceProperties Properties{};
    vkGetPhysicalDeviceProperties(Gpu.GetPhysical(), &Properties);
    GpuName = Properties.deviceName;

    ImGui::CreateContext();
    ImGuiIO& Io = ImGui::GetIO();
    Io.LogFilename = nullptr;
    Io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    Io.ConfigWindowsMoveFromTitleBarOnly = true;

    Fonts = LoadEditorFonts();
    ApplyEditorTheme();
    RegisterSettingsHandler();

    ImGui_ImplVulkan_InitInfo InitInfo = {};
    InitInfo.ApiVersion = VK_API_VERSION_1_3;
    InitInfo.Instance = Instance.GetInstance();
    InitInfo.PhysicalDevice = Gpu.GetPhysical();
    InitInfo.Device = Gpu.GetHandle();
    InitInfo.QueueFamily = Gpu.GetQueueFamily();
    InitInfo.Queue = Gpu.GetQueue();
    InitInfo.RenderPass = Pass;
    InitInfo.Subpass = 0;
    // The backend owns its pool; each texture the UI shows takes one set from it
    InitInfo.DescriptorPoolSize = kUiTextureSets;
    InitInfo.MinImageCount = 2;
    InitInfo.ImageCount = ImageCount;
    InitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&InitInfo);
    ImGui_ImplGlfw_InitForVulkan(Target.GetHandle(), true);

    LastUseSlang = VK.GetUseSlang();
    LastUseRaytracer = VK.GetSettings().UseRaytracer;

    Log(LogLevel::kInfo, "GPU: %s, Vulkan %u.%u.%u", GpuName.c_str(),
        VK_API_VERSION_MAJOR(Properties.apiVersion),
        VK_API_VERSION_MINOR(Properties.apiVersion),
        VK_API_VERSION_PATCH(Properties.apiVersion));
    const Scene& World = VK.GetScene();
    uint64_t Triangles = 0;
    size_t MaterialCount = 0;
    for (const Mesh& Model : World.GetMeshes())
    {
        Triangles += Model.NbIndices / 3;
        MaterialCount += Model.Materials.size();
        Log(LogLevel::kInfo, "Loaded model '%s': %s vertices, %s triangles",
            Model.Name.c_str(), GroupDigits(Model.NbVertices).c_str(),
            GroupDigits(Model.NbIndices / 3).c_str());
    }
    size_t EmissiveCount = 0;
    for (const Mesh& Model : World.GetMeshes())
    {
        for (const Material& Value : Model.Materials)
        {
            EmissiveCount += Value.Emission.x > 0.0f || Value.Emission.y > 0.0f ||
                                     Value.Emission.z > 0.0f
                                 ? 1
                                 : 0;
        }
    }
    Log(LogLevel::kInfo,
        "Scene: %zu %s, %s triangles, %zu materials (%zu emissive), %zu textures",
        World.GetMeshes().size(), World.GetMeshes().size() == 1 ? "mesh" : "meshes",
        GroupDigits(Triangles).c_str(), MaterialCount, EmissiveCount,
        World.GetTextures().size());
    Log(LogLevel::kInfo, "Shaders: %s", VK.GetUseSlang() ? "Slang" : "GLSL (generated)");
}

void Gui::Shutdown()
{
    RTRT_CHECK_VK(vkDeviceWaitIdle(DeviceHandle));
    if (ViewportTexture != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(ViewportTexture);
        ViewportTexture = VK_NULL_HANDLE;
    }
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Gui::RegisterSettingsHandler()
{
    ImGuiSettingsHandler Handler;
    Handler.TypeName = "Editor";
    Handler.TypeHash = ImHashStr("Editor");
    Handler.UserData = this;
    Handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* Self,
                            const char* Name) -> void*
    { return strcmp(Name, "State") == 0 ? Self->UserData : nullptr; };
    Handler.ReadLineFn =
        [](ImGuiContext*, ImGuiSettingsHandler*, void* Entry, const char* Line)
    { static_cast<Gui*>(Entry)->ReadSetting(Line); };
    Handler.WriteAllFn =
        [](ImGuiContext*, ImGuiSettingsHandler* Self, ImGuiTextBuffer* Out)
    { static_cast<Gui*>(Self->UserData)->WriteSettings(Out); };
    ImGui::AddSettingsHandler(&Handler);
}

void Gui::ReadSetting(const char* Line)
{
    constexpr char kPanels[] = "Panels=";
    constexpr char kSelection[] = "Selection=";
    if (strncmp(Line, kPanels, sizeof(kPanels) - 1) == 0)
    {
        const long Value = strtol(Line + sizeof(kPanels) - 1, nullptr, 10);
        ShowOutliner = (Value & 1) != 0;
        ShowInspector = (Value & 2) != 0;
        ShowRenderSettings = (Value & 4) != 0;
        ShowStats = (Value & 8) != 0;
        ShowConsole = (Value & 16) != 0;
    }
    else if (strncmp(Line, kSelection, sizeof(kSelection) - 1) == 0)
    {
        char* Next = nullptr;
        const long Kind = strtol(Line + sizeof(kSelection) - 1, &Next, 10);
        const long Mesh = *Next == ',' ? strtol(Next + 1, &Next, 10) : 0;
        const long MaterialIndex = *Next == ',' ? strtol(Next + 1, &Next, 10) : 0;
        if (Kind >= static_cast<long>(Selection::kNone) &&
            Kind <= static_cast<long>(Selection::kMaterial))
        {
            Selected = static_cast<Selection>(Kind);
            SelectedMesh = static_cast<uint32_t>(std::max(Mesh, 0L));
            SelectedMaterial = static_cast<uint32_t>(std::max(MaterialIndex, 0L));
        }
    }
}

void Gui::WriteSettings(ImGuiTextBuffer* Out) const
{
    const int Panels = (ShowOutliner ? 1 : 0) | (ShowInspector ? 2 : 0) |
                       (ShowRenderSettings ? 4 : 0) | (ShowStats ? 8 : 0) |
                       (ShowConsole ? 16 : 0);
    Out->appendf("[Editor][State]\nPanels=%d\nSelection=%d,%u,%u\n\n", Panels,
                 static_cast<int>(Selected), SelectedMesh, SelectedMaterial);
}

bool Gui::WantsTextInput()
{
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput;
}

void Gui::Log(LogLevel Level, const char* Format, ...)
{
    char Buffer[512];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Buffer, sizeof(Buffer), Format, Args);
    va_end(Args);
    Messages.Add(glfwGetTime() - StartTime, Level, "%s", Buffer);
}

void Gui::Draw(Renderer& VK, Camera& View, CameraController& Controller)
{
    const double Start = glfwGetTime();
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    const double BuildStart = glfwGetTime();
    LastDisplayWaitMs = static_cast<float>((BuildStart - Start) * 1000.0);
    ImGui::NewFrame();

    SyncViewportTexture(VK);
    TrackRendererEvents(VK);
    HandleShortcuts(VK);

    if (!ShowUi)
    {
        DrawFullWindowViewport(VK);
        LastUiMs = static_cast<float>((glfwGetTime() - BuildStart) * 1000.0);
        return;
    }

    ImGui::PushFont(Fonts.Ui);
    DrawMenuBar(VK, View);
    DrawStatusBar(VK);
    DrawDockSpace();

    DrawViewport(VK, Controller);
    if (ShowOutliner)
    {
        DrawOutliner(VK);
    }
    if (ShowInspector)
    {
        DrawInspector(VK, View, Controller);
    }
    if (ShowRenderSettings)
    {
        DrawRenderSettings(VK);
    }
    if (ShowConsole)
    {
        DrawConsole();
    }
    if (ShowStats)
    {
        DrawStats(VK);
    }
    DrawControlsPopup();
    ImGui::PopFont();
    LastUiMs = static_cast<float>((glfwGetTime() - BuildStart) * 1000.0);
}

void Gui::HandleShortcuts(Renderer& VK)
{
    if (WantsTextInput())
    {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false))
    {
        ShowUi = !ShowUi;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F12, false))
    {
        RequestScreenshot(VK);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
    {
        VK.ResetRtAccumulation();
        Log(LogLevel::kInfo, "Accumulation reset");
    }
}

void Gui::RequestScreenshot(Renderer& VK)
{
    VK.RequestScreenshot(ScreenshotPath());
}

void Gui::TrackRendererEvents(const Renderer& VK)
{
    const Screenshot& Capture = VK.GetScreenshot();
    if (Capture.GetSaveCount() != ScreenshotsSeen)
    {
        ScreenshotsSeen = Capture.GetSaveCount();
        StatusMessageTime = glfwGetTime();
        StatusMessageOk = Capture.GetLastSucceeded();
        StatusMessage = Capture.GetLastSucceeded() ? "Saved " + Capture.GetLastResult()
                                                   : Capture.GetLastResult();
        if (Capture.GetLastSucceeded())
        {
            Log(LogLevel::kInfo, "Saved screenshot %s", Capture.GetLastResult().c_str());
        }
        else
        {
            Log(LogLevel::kError, "%s", Capture.GetLastResult().c_str());
        }
    }

    if (VK.GetUseSlang() != LastUseSlang)
    {
        LastUseSlang = VK.GetUseSlang();
        Log(LogLevel::kInfo, "Switched shaders to %s",
            LastUseSlang ? "Slang" : "GLSL (generated)");
    }

    if (VK.GetSettings().UseRaytracer != LastUseRaytracer)
    {
        LastUseRaytracer = VK.GetSettings().UseRaytracer;
        Log(LogLevel::kInfo, "Renderer: %s",
            LastUseRaytracer ? "path tracer" : "rasterizer");
    }

    const VkExtent2D Extent = VK.GetRenderExtent();
    if ((Extent.width != LoggedExtent.width || Extent.height != LoggedExtent.height) &&
        glfwGetTime() - ExtentChangedAt > kResizeLogDelay)
    {
        LoggedExtent = Extent;
        Log(LogLevel::kInfo, "Render resolution %u x %u", Extent.width, Extent.height);
    }
}

void Gui::DrawMenuBar(Renderer& VK, Camera& View)
{
    if (!ImGui::BeginMainMenuBar())
    {
        return;
    }
    RenderSettings& Settings = VK.GetSettings();

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Save Screenshot", "F12"))
        {
            RequestScreenshot(VK);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Esc"))
        {
            ExitRequested = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Outliner", nullptr, &ShowOutliner);
        ImGui::MenuItem("Inspector", nullptr, &ShowInspector);
        ImGui::MenuItem("Render Settings", nullptr, &ShowRenderSettings);
        ImGui::MenuItem("Stats", nullptr, &ShowStats);
        ImGui::MenuItem("Console", nullptr, &ShowConsole);
        ImGui::Separator();
        if (ImGui::MenuItem("Hide Interface", "F1"))
        {
            ShowUi = false;
        }
        if (ImGui::MenuItem("Reset Layout"))
        {
            ResetLayout = true;
            ShowOutliner = true;
            ShowInspector = true;
            ShowRenderSettings = true;
            ShowStats = true;
            ShowConsole = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Camera"))
        {
            View.Reset();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Render"))
    {
        if (ImGui::MenuItem("Path Tracer", nullptr, Settings.UseRaytracer))
        {
            if (!Settings.UseRaytracer)
            {
                VK.ResetRtAccumulation();
            }
            Settings.UseRaytracer = true;
        }
        if (ImGui::MenuItem("Rasterizer", nullptr, !Settings.UseRaytracer))
        {
            Settings.UseRaytracer = false;
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("View Mode", Settings.UseRaytracer))
        {
            for (int I = 0; I < kViewModeCount; I++)
            {
                if (ImGui::MenuItem(kViewModeNames[I], nullptr, Settings.ViewMode == I))
                {
                    Settings.ViewMode = I;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::MenuItem("Denoiser", nullptr, &Settings.DenoiseEnabled,
                        Settings.UseRaytracer);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Accumulation", "F5"))
        {
            VK.ResetRtAccumulation();
            Log(LogLevel::kInfo, "Accumulation reset");
        }
        if (ImGui::MenuItem("Restore Default Settings"))
        {
            VK.SetDefaultSettings();
            Log(LogLevel::kInfo, "Render settings restored to defaults");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("Controls"))
        {
            OpenControls = true;
        }
        ImGui::EndMenu();
    }

    const float NameWidth = ImGui::CalcTextSize(GpuName.c_str()).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - NameWidth -
                    ImGui::GetStyle().WindowPadding.x * 2.0f);
    ImGui::TextDisabled("%s", GpuName.c_str());
    ImGui::EndMainMenuBar();
}

void Gui::DrawStatusBar(const Renderer& VK)
{
    const ImGuiWindowFlags Flags = ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_MenuBar;
    if (!ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(),
                                     ImGuiDir_Down, ImGui::GetFrameHeight(), Flags))
    {
        ImGui::End();
        return;
    }
    if (ImGui::BeginMenuBar())
    {
        const RenderSettings& Settings = VK.GetSettings();
        const VkExtent2D Extent = VK.GetRenderExtent();
        ImGui::PushFont(Fonts.Mono);
        ImGui::TextUnformatted(Settings.UseRaytracer ? "Path tracer" : "Rasterizer");
        ImGui::TextDisabled("|");
        ImGui::TextUnformatted(VK.GetUseSlang() ? "Slang" : "GLSL");
        ImGui::TextDisabled("|");
        ImGui::TextUnformatted("VSync");
        ImGui::TextDisabled("|");
        ImGui::Text("%u x %u px", Extent.width, Extent.height);
        if (Settings.UseRaytracer)
        {
            ImGui::TextDisabled("|");
            ImGui::Text("%d spp", VK.GetFrameCount());
        }
        if (!StatusMessage.empty() &&
            glfwGetTime() - StatusMessageTime < kStatusMessageSeconds)
        {
            ImGui::TextDisabled("|");
            ImGui::TextColored(StatusMessageOk ? kGood : kError, "%s",
                               StatusMessage.c_str());
        }

        const FrameStats::Summary Frame = Stats.Summarize(Stats.FrameMs);
        const FrameStats::Summary Gpu = Stats.Summarize(Stats.GpuMs);
        char Right[128];
        snprintf(Right, sizeof(Right), "GPU %.2f ms   Frame %.2f ms (%.0f fps)", Gpu.Avg,
                 Frame.Avg, Frame.Avg > 0.0f ? 1000.0f / Frame.Avg : 0.0f);
        const float Width = ImGui::CalcTextSize(Right).x;
        ImGui::SameLine(ImGui::GetWindowWidth() - Width -
                        ImGui::GetStyle().WindowPadding.x * 2.0f);
        ImGui::TextUnformatted(Right);
        ImGui::PopFont();
        ImGui::EndMenuBar();
    }
    ImGui::End();
}

void Gui::DrawDockSpace()
{
    const ImGuiID DockId = ImGui::GetID("EditorDockSpace");
    if (ResetLayout || ImGui::DockBuilderGetNode(DockId) == nullptr)
    {
        BuildDefaultLayout(DockId);
        ResetLayout = false;
    }
    ImGui::DockSpaceOverViewport(DockId, ImGui::GetMainViewport());
}

void Gui::BuildDefaultLayout(ImGuiID DockId)
{
    const ImGuiViewport* Viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(DockId);
    ImGui::DockBuilderAddNode(DockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(DockId, Viewport->WorkSize);

    ImGuiID Center = DockId;
    const ImGuiID Left =
        ImGui::DockBuilderSplitNode(Center, ImGuiDir_Left, 0.18f, nullptr, &Center);
    const ImGuiID Right =
        ImGui::DockBuilderSplitNode(Center, ImGuiDir_Right, 0.30f, nullptr, &Center);
    const ImGuiID Bottom =
        ImGui::DockBuilderSplitNode(Center, ImGuiDir_Down, 0.27f, nullptr, &Center);

    ImGui::DockBuilderDockWindow("Outliner", Left);
    ImGui::DockBuilderDockWindow("Render Settings", Right);
    ImGui::DockBuilderDockWindow("Inspector", Right);
    ImGui::DockBuilderDockWindow("Stats", Bottom);
    ImGui::DockBuilderDockWindow("Console", Bottom);
    ImGui::DockBuilderDockWindow("Viewport", Center);
    if (ImGuiDockNode* CenterNode = ImGui::DockBuilderGetNode(Center))
    {
        CenterNode->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
    }
    ImGui::DockBuilderFinish(DockId);
    Log(LogLevel::kInfo, "Editor layout reset");
}

void Gui::DrawControlsPopup()
{
    if (OpenControls)
    {
        ImGui::OpenPopup("Controls");
        OpenControls = false;
    }
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Controls", nullptr, ImGuiWindowFlags_NoResize))
    {
        return;
    }
    constexpr const char* kRows[][2] = {
        {"Look around", "Left-drag in the viewport"},
        {"Move", "W A S D, E up, C down"},
        {"Move speed", "Mouse wheel over the viewport"},
        {"Hide or show the interface", "F1"},
        {"Reset accumulation", "F5"},
        {"Save screenshot", "F12"},
        {"Exit", "Esc"},
    };
    if (ImGui::BeginTable("Controls", 2, ImGuiTableFlags_RowBg))
    {
        for (const auto& Row : kRows)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(Row[0]);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%s", Row[1]);
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    if (ImGui::Button("Close", ImVec2(-FLT_MIN, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void Gui::SyncViewportTexture(const Renderer& VK)
{
    if (ViewportTexture != VK_NULL_HANDLE && ViewportVersion == VK.GetViewportVersion())
    {
        return;
    }
    if (ViewportTexture != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(ViewportTexture);
    }
    const VkDescriptorImageInfo Info = VK.GetViewportImage().Descriptor();
    ViewportTexture =
        ImGui_ImplVulkan_AddTexture(Info.sampler, Info.imageView, Info.imageLayout);
    ViewportVersion = VK.GetViewportVersion();
    ExtentChangedAt = glfwGetTime();
}

void Gui::DrawViewport(Renderer& VK, CameraController& Controller)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool Visible =
        ImGui::Begin("Viewport", nullptr,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                         ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();
    if (!Visible)
    {
        ViewportDragging = false;
        ViewportActive = false;
        ImGui::End();
        return;
    }

    const ImGuiIO& Io = ImGui::GetIO();
    ImVec2 Size = ImGui::GetContentRegionAvail();
    Size.x = std::max(Size.x, 1.0f);
    Size.y = std::max(Size.y, 1.0f);
    VK.RequestRenderExtent(
        {static_cast<uint32_t>(Size.x * Io.DisplayFramebufferScale.x),
         static_cast<uint32_t>(Size.y * Io.DisplayFramebufferScale.y)});

    const ImVec2 Origin = ImGui::GetCursorScreenPos();
    ImGui::Image(ToTextureId(ViewportTexture), Size);
    ImGui::SetCursorScreenPos(Origin);
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##ViewportInput", Size, ImGuiButtonFlags_MouseButtonLeft);
    const bool Hovered = ImGui::IsItemHovered();
    ViewportDragging = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    ViewportActive = Hovered || ViewportDragging ||
                     ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (Hovered && Io.MouseWheel != 0.0f)
    {
        const float Step = Io.MouseWheel > 0.0f ? 1.15f : 1.0f / 1.15f;
        Controller.SetMoveSpeed(
            std::clamp(Controller.GetMoveSpeed() * Step, 0.05f, 20.0f));
    }

    DrawViewportOverlay(VK, Origin, Size);
    ImGui::End();
}

void Gui::DrawViewportOverlay(Renderer& VK, const ImVec2& Origin, const ImVec2& Size)
{
    RenderSettings& Settings = VK.GetSettings();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    // The toolbar goes on channel 1 so its background can go under it once its size is
    // known.
    ImDrawListSplitter Splitter;
    Splitter.Split(DrawList, 2);
    Splitter.SetCurrentChannel(DrawList, 1);

    const ImVec2 ToolbarPos(Origin.x + kOverlayMargin, Origin.y + kOverlayMargin);
    ImGui::SetCursorScreenPos(ImVec2(ToolbarPos.x + 5.0f, ToolbarPos.y + 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 2.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.22f, 0.23f, 0.24f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.23f, 0.24f, 0.9f));

    int Mode = Settings.UseRaytracer ? 0 : 1;
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Path tracer").x + 34.0f);
    if (ImGui::Combo("##Renderer", &Mode, "Path tracer\0Rasterizer\0"))
    {
        if (Mode == 0 && !Settings.UseRaytracer)
        {
            VK.ResetRtAccumulation();
        }
        Settings.UseRaytracer = Mode == 0;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::SetTooltip("Renderer");
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!Settings.UseRaytracer);
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Sample count").x + 34.0f);
    ImGui::Combo("##View", &Settings.ViewMode, kViewModeNames, kViewModeCount);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                             ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("View mode: the final image or one G-buffer channel");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Denoise", &Settings.DenoiseEnabled);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        VK.ResetRtAccumulation();
        Log(LogLevel::kInfo, "Accumulation reset");
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::SetTooltip("Reset accumulation (F5)");
    }
    ImGui::SameLine();
    if (ImGui::Button("Screenshot"))
    {
        RequestScreenshot(VK);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::SetTooltip("Save the viewport as a PNG in screenshots/ (F12)");
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    const ImVec2 ToolbarEnd(ImGui::GetItemRectMax().x + 5.0f,
                            ImGui::GetItemRectMax().y + 4.0f);
    Splitter.SetCurrentChannel(DrawList, 0);
    DrawList->AddRectFilled(ToolbarPos, ToolbarEnd, OverlayBg(), 4.0f);
    Splitter.Merge(DrawList);

    const FrameStats::Summary Frame = Stats.Summarize(Stats.FrameMs);
    const VkExtent2D Extent = VK.GetRenderExtent();
    const float MaxSamples = VK.GetRayConstants().MaxSamples;
    const bool Converged = Settings.UseRaytracer && MaxSamples > 0.0f &&
                           static_cast<float>(VK.GetFrameCount()) >= MaxSamples;
    char Lines[3][64];
    snprintf(Lines[0], sizeof(Lines[0]), "%.1f fps  %.2f ms",
             Frame.Avg > 0.0f ? 1000.0f / Frame.Avg : 0.0f, Frame.Avg);
    if (!Settings.UseRaytracer)
    {
        snprintf(Lines[1], sizeof(Lines[1]), "rasterized");
    }
    else if (MaxSamples > 0.0f)
    {
        snprintf(Lines[1], sizeof(Lines[1]), "%d / %.0f spp", VK.GetFrameCount(),
                 MaxSamples);
    }
    else
    {
        snprintf(Lines[1], sizeof(Lines[1]), "%d spp", VK.GetFrameCount());
    }
    snprintf(Lines[2], sizeof(Lines[2]), "%u x %u", Extent.width, Extent.height);

    ImGui::PushFont(Fonts.Mono);
    const float LineHeight = ImGui::GetTextLineHeight();
    float Width = 0.0f;
    for (const char* Line : Lines)
    {
        Width = std::max(Width, ImGui::CalcTextSize(Line).x);
    }
    const ImVec2 StatsMax(Origin.x + Size.x - kOverlayMargin,
                          Origin.y + kOverlayMargin + LineHeight * 3.0f + 10.0f);
    const ImVec2 StatsMin(StatsMax.x - Width - 16.0f, Origin.y + kOverlayMargin);
    DrawList->AddRectFilled(StatsMin, StatsMax, OverlayBg(), 4.0f);
    const ImU32 Text = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 LineColors[3] = {Text, Converged ? ImGui::GetColorU32(kGood) : Text,
                                 Text};
    for (int I = 0; I < 3; I++)
    {
        const float LineWidth = ImGui::CalcTextSize(Lines[I]).x;
        DrawList->AddText(
            ImVec2(StatsMax.x - 8.0f - LineWidth, StatsMin.y + 5.0f + LineHeight * I),
            LineColors[I], Lines[I]);
    }
    ImGui::PopFont();
}

void Gui::DrawFullWindowViewport(Renderer& VK)
{
    const ImGuiIO& Io = ImGui::GetIO();
    ImGui::GetBackgroundDrawList()->AddImage(ToTextureId(ViewportTexture),
                                             ImVec2(0.0f, 0.0f), Io.DisplaySize);
    VK.RequestRenderExtent(
        {static_cast<uint32_t>(Io.DisplaySize.x * Io.DisplayFramebufferScale.x),
         static_cast<uint32_t>(Io.DisplaySize.y * Io.DisplayFramebufferScale.y)});
    ViewportDragging = ImGui::IsMouseDown(ImGuiMouseButton_Left) && !Io.WantCaptureMouse;
    ViewportActive = true;
}

}  // namespace rtrt
