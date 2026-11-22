#include "ui/theme.h"

#include <filesystem>
#include <string>
#include <system_error>

#include <imgui.h>

namespace rtrt
{

namespace
{

constexpr float kUiFontSize = 16.0f;
constexpr float kMonoFontSize = 15.0f;
constexpr float kTitleFontSize = 21.0f;

ImFont* AddFontIfPresent(const std::string& FileName, float Size)
{
    std::error_code Error;
    if (!std::filesystem::exists(FileName, Error))
    {
        return nullptr;
    }
    return ImGui::GetIO().Fonts->AddFontFromFileTTF(FileName.c_str(), Size);
}

ImVec4 Gray(float Value, float Alpha = 1.0f)
{
    return ImVec4(Value, Value, Value * 1.04f, Alpha);
}

ImVec4 WithAlpha(const ImVec4& Color, float Alpha)
{
    return ImVec4(Color.x, Color.y, Color.z, Alpha);
}

}  // namespace

EditorFonts LoadEditorFonts()
{
    const std::string Folder = "C:/Windows/Fonts/";
    EditorFonts Fonts;
    Fonts.Ui = AddFontIfPresent(Folder + "segoeui.ttf", kUiFontSize);
    if (Fonts.Ui == nullptr)
    {
        Fonts.Ui = ImGui::GetIO().Fonts->AddFontDefault();
    }
    Fonts.Bold = AddFontIfPresent(Folder + "seguisb.ttf", kUiFontSize);
    if (Fonts.Bold == nullptr)
    {
        Fonts.Bold = AddFontIfPresent(Folder + "segoeuib.ttf", kUiFontSize);
    }
    Fonts.Title = AddFontIfPresent(Folder + "seguisb.ttf", kTitleFontSize);
    if (Fonts.Title == nullptr)
    {
        Fonts.Title = AddFontIfPresent(Folder + "segoeuib.ttf", kTitleFontSize);
    }
    Fonts.Mono = AddFontIfPresent(Folder + "consola.ttf", kMonoFontSize);
    if (Fonts.Bold == nullptr)
    {
        Fonts.Bold = Fonts.Ui;
    }
    if (Fonts.Title == nullptr)
    {
        Fonts.Title = Fonts.Bold;
    }
    if (Fonts.Mono == nullptr)
    {
        Fonts.Mono = Fonts.Ui;
    }
    return Fonts;
}

void ApplyEditorTheme()
{
    ImGuiStyle& Style = ImGui::GetStyle();
    Style.WindowPadding = ImVec2(8.0f, 8.0f);
    Style.FramePadding = ImVec2(6.0f, 3.0f);
    Style.CellPadding = ImVec2(6.0f, 3.0f);
    Style.ItemSpacing = ImVec2(6.0f, 5.0f);
    Style.ItemInnerSpacing = ImVec2(5.0f, 4.0f);
    Style.IndentSpacing = 14.0f;
    Style.ScrollbarSize = 12.0f;
    // A thin grab keeps the value text readable
    Style.GrabMinSize = 4.0f;
    Style.WindowBorderSize = 1.0f;
    Style.FrameBorderSize = 0.0f;
    Style.PopupBorderSize = 1.0f;
    Style.TabBorderSize = 0.0f;
    Style.WindowRounding = 3.0f;
    Style.ChildRounding = 3.0f;
    Style.FrameRounding = 3.0f;
    Style.PopupRounding = 3.0f;
    Style.ScrollbarRounding = 3.0f;
    Style.GrabRounding = 2.0f;
    Style.TabRounding = 3.0f;
    Style.WindowMenuButtonPosition = ImGuiDir_None;
    Style.SeparatorTextBorderSize = 1.0f;
    Style.DockingSeparatorSize = 2.0f;

    ImVec4* Colors = Style.Colors;
    Colors[ImGuiCol_Text] = Gray(0.88f);
    Colors[ImGuiCol_TextDisabled] = kTextDim;
    Colors[ImGuiCol_WindowBg] = Gray(0.135f);
    Colors[ImGuiCol_ChildBg] = Gray(0.135f, 0.0f);
    Colors[ImGuiCol_PopupBg] = Gray(0.11f, 0.98f);
    Colors[ImGuiCol_Border] = Gray(0.06f);
    Colors[ImGuiCol_BorderShadow] = Gray(0.0f, 0.0f);
    Colors[ImGuiCol_FrameBg] = Gray(0.20f);
    Colors[ImGuiCol_FrameBgHovered] = Gray(0.25f);
    Colors[ImGuiCol_FrameBgActive] = Gray(0.28f);
    Colors[ImGuiCol_TitleBg] = Gray(0.10f);
    Colors[ImGuiCol_TitleBgActive] = Gray(0.10f);
    Colors[ImGuiCol_TitleBgCollapsed] = Gray(0.10f);
    Colors[ImGuiCol_MenuBarBg] = Gray(0.10f);
    Colors[ImGuiCol_ScrollbarBg] = Gray(0.12f, 0.0f);
    Colors[ImGuiCol_ScrollbarGrab] = Gray(0.28f);
    Colors[ImGuiCol_ScrollbarGrabHovered] = Gray(0.34f);
    Colors[ImGuiCol_ScrollbarGrabActive] = Gray(0.40f);
    Colors[ImGuiCol_CheckMark] = kAccent;
    Colors[ImGuiCol_SliderGrab] = WithAlpha(kAccent, 0.75f);
    Colors[ImGuiCol_SliderGrabActive] = kAccent;
    Colors[ImGuiCol_Button] = Gray(0.23f);
    Colors[ImGuiCol_ButtonHovered] = Gray(0.29f);
    Colors[ImGuiCol_ButtonActive] = WithAlpha(kAccent, 0.55f);
    Colors[ImGuiCol_Header] = WithAlpha(kAccent, 0.30f);
    Colors[ImGuiCol_HeaderHovered] = Gray(0.24f);
    Colors[ImGuiCol_HeaderActive] = WithAlpha(kAccent, 0.40f);
    Colors[ImGuiCol_Separator] = Gray(0.22f);
    Colors[ImGuiCol_SeparatorHovered] = WithAlpha(kAccent, 0.6f);
    Colors[ImGuiCol_SeparatorActive] = kAccent;
    Colors[ImGuiCol_ResizeGrip] = Gray(0.0f, 0.0f);
    Colors[ImGuiCol_ResizeGripHovered] = WithAlpha(kAccent, 0.5f);
    Colors[ImGuiCol_ResizeGripActive] = kAccent;
    Colors[ImGuiCol_Tab] = Gray(0.11f);
    Colors[ImGuiCol_TabHovered] = Gray(0.22f);
    Colors[ImGuiCol_TabSelected] = Gray(0.165f);
    Colors[ImGuiCol_TabSelectedOverline] = kAccent;
    Colors[ImGuiCol_TabDimmed] = Gray(0.11f);
    Colors[ImGuiCol_TabDimmedSelected] = Gray(0.145f);
    Colors[ImGuiCol_TabDimmedSelectedOverline] = Gray(0.35f);
    Colors[ImGuiCol_DockingPreview] = WithAlpha(kAccent, 0.5f);
    Colors[ImGuiCol_DockingEmptyBg] = Gray(0.07f);
    Colors[ImGuiCol_PlotLines] = kAccent;
    Colors[ImGuiCol_PlotLinesHovered] = kWarn;
    Colors[ImGuiCol_PlotHistogram] = WithAlpha(kAccent, 0.8f);
    Colors[ImGuiCol_PlotHistogramHovered] = kAccent;
    Colors[ImGuiCol_TableHeaderBg] = Gray(0.17f);
    Colors[ImGuiCol_TableBorderStrong] = Gray(0.08f);
    Colors[ImGuiCol_TableBorderLight] = Gray(0.18f);
    Colors[ImGuiCol_TableRowBg] = Gray(0.0f, 0.0f);
    Colors[ImGuiCol_TableRowBgAlt] = Gray(1.0f, 0.025f);
    Colors[ImGuiCol_TextSelectedBg] = WithAlpha(kAccent, 0.35f);
    Colors[ImGuiCol_NavCursor] = kAccent;
    Colors[ImGuiCol_ModalWindowDimBg] = Gray(0.0f, 0.55f);
}

}  // namespace rtrt
