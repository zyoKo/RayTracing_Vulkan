#include "ui/property_grid.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ui/theme.h"

namespace rtrt
{

namespace
{

constexpr float kLabelWidth = 118.0f;

}  // namespace

bool BeginProperties(const char* Id)
{
    if (!ImGui::BeginTable(Id, 2, ImGuiTableFlags_SizingStretchProp))
    {
        return false;
    }
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, kLabelWidth);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

void EndProperties()
{
    ImGui::EndTable();
}

void PropertyRow(const char* Label, const char* Tooltip)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextUnformatted(Label);
    ImGui::PopStyleColor();
    if (Tooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::SetTooltip("%s", Tooltip);
    }
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

void PropertyText(const char* Label, const char* Format, ...)
{
    PropertyRow(Label);
    va_list Args;
    va_start(Args, Format);
    ImGui::TextV(Format, Args);
    va_end(Args);
}

bool Section(ImFont* HeaderFont, const char* Label, bool DefaultOpen)
{
    ImGui::PushFont(HeaderFont);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.19f, 0.195f, 0.205f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.23f, 0.235f, 0.245f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.26f, 0.265f, 0.275f, 1.0f));
    const bool Open = ImGui::CollapsingHeader(
        Label, (DefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0) |
                   ImGuiTreeNodeFlags_SpanAvailWidth);
    ImGui::PopStyleColor(3);
    ImGui::PopFont();
    return Open;
}

bool ScalarProperty(const char* Id, float* Value, float Min, float Max,
                    const char* Format, ImGuiSliderFlags Flags)
{
    float Fraction = 0.0f;
    if ((Flags & ImGuiSliderFlags_Logarithmic) != 0)
    {
        if (Min > 0.0f)
        {
            Fraction = std::log(std::max(*Value, Min) / Min) / std::log(Max / Min);
        }
        else
        {
            const float Scale = 1000.0f / Max;
            Fraction =
                std::log1p(std::max(*Value, 0.0f) * Scale) / std::log1p(Max * Scale);
        }
    }
    else
    {
        Fraction = (*Value - Min) / (Max - Min);
    }
    Fraction = std::clamp(Fraction, 0.0f, 1.0f);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImDrawListSplitter Splitter;
    Splitter.Split(DrawList, 2);
    Splitter.SetCurrentChannel(DrawList, 1);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.05f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
    const float Speed = (Flags & ImGuiSliderFlags_Logarithmic) != 0
                            ? 0.002f * (Max - Min)
                            : (Max - Min) / 400.0f;
    const bool Changed = ImGui::DragFloat(Id, Value, Speed, Min, Max, Format,
                                          Flags | ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopStyleColor(3);

    const ImVec2 RectMin = ImGui::GetItemRectMin();
    const ImVec2 RectMax = ImGui::GetItemRectMax();
    Splitter.SetCurrentChannel(DrawList, 0);
    const float Rounding = ImGui::GetStyle().FrameRounding;
    DrawList->AddRectFilled(RectMin, RectMax, ImGui::GetColorU32(ImGuiCol_FrameBg),
                            Rounding);
    if (Fraction > 0.0f)
    {
        DrawList->AddRectFilled(
            RectMin, ImVec2(RectMin.x + (RectMax.x - RectMin.x) * Fraction, RectMax.y),
            ImGui::GetColorU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.28f)), Rounding);
    }
    Splitter.Merge(DrawList);
    return Changed;
}

bool AxisProperty(const char* Id, float* Values, int Count, const char* const* Prefixes,
                  float Speed, const char* Format)
{
    ImGui::PushID(Id);
    const float Spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    const float Width =
        (ImGui::GetContentRegionAvail().x - Spacing * static_cast<float>(Count - 1)) /
        static_cast<float>(Count);
    bool Changed = false;
    for (int I = 0; I < Count; I++)
    {
        if (I > 0)
        {
            ImGui::SameLine(0.0f, Spacing);
        }
        char ComponentFormat[32];
        if (Prefixes[I][0] == 0)
        {
            snprintf(ComponentFormat, sizeof(ComponentFormat), "%s", Format);
        }
        else
        {
            snprintf(ComponentFormat, sizeof(ComponentFormat), "%s %s", Prefixes[I],
                     Format);
        }
        ImGui::PushID(I);
        ImGui::SetNextItemWidth(Width);
        Changed |=
            ImGui::DragFloat("##Axis", &Values[I], Speed, 0.0f, 0.0f, ComponentFormat);
        ImGui::PopID();
    }
    ImGui::PopID();
    return Changed;
}

std::string GroupDigits(uint64_t Value)
{
    std::string Digits = std::to_string(Value);
    for (int I = static_cast<int>(Digits.size()) - 3; I > 0; I -= 3)
    {
        Digits.insert(static_cast<size_t>(I), ",");
    }
    return Digits;
}

void HelpMarker(const char* Text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::SetTooltip("%s", Text);
    }
}

}  // namespace rtrt
