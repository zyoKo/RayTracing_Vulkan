#include "ui/property_grid.h"

#include <cfloat>
#include <cstdarg>

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
