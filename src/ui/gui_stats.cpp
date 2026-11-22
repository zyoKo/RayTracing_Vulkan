#include "ui/gui.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ui/theme.h"

namespace rtrt
{

namespace
{

constexpr float kGraphMinHeight = 36.0f;

// A right-aligned number in the current table cell.
void RightAligned(const char* Format, float Value)
{
    char Text[32];
    snprintf(Text, sizeof(Text), Format, Value);
    const float Width = ImGui::CalcTextSize(Text).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - Width);
    ImGui::TextUnformatted(Text);
}

void TimingRow(const FrameStats& Stats, const char* Label, const char* Tooltip,
               const float* Values)
{
    const FrameStats::Summary Range = Stats.Summarize(Values);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(Label);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::SetTooltip("%s", Tooltip);
    }
    const float Columns[] = {Stats.Latest(Values), Range.Avg, Range.Lo, Range.Hi};
    for (int I = 0; I < 4; I++)
    {
        ImGui::TableSetColumnIndex(I + 1);
        RightAligned("%.2f", Columns[I]);
    }
}

}  // namespace

void Gui::DrawStats(const Renderer& VK)
{
    if (!ImGui::Begin("Stats", &ShowStats))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTable("StatsLayout", 3,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Frame", ImGuiTableColumnFlags_WidthStretch, 1.15f);
        ImGui::TableSetupColumn("Passes", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Graphs", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::PushFont(Fonts.Mono);
        const ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg |
                                           ImGuiTableFlags_BordersOuterH |
                                           ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("Timings", 5, TableFlags))
        {
            ImGui::TableSetupColumn("Frame (ms)", ImGuiTableColumnFlags_WidthStretch, 1.6f);
            ImGui::TableSetupColumn("Now", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Min", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();
            TimingRow(Stats, "Frame", "Wall time from one frame to the next", Stats.FrameMs);
            TimingRow(Stats, "CPU", "Input, camera and UI on the main thread", Stats.CpuMs);
            TimingRow(Stats, "Record", "Acquire, record, submit and present", Stats.DrawMs);
            TimingRow(Stats, "GPU wait", "Blocked on the previous frame's fence",
                      Stats.GpuWaitMs);
            TimingRow(Stats, "GPU", "First to last timestamp of the frame", Stats.GpuMs);
            ImGui::EndTable();
        }

        ImGui::TableSetColumnIndex(1);
        const GpuTimer& Timer = VK.GetGpuTimer();
        const float GpuTotal = std::max(Timer.GetFrameMs(), 0.001f);
        if (ImGui::BeginTable("Passes", 3, TableFlags))
        {
            ImGui::TableSetupColumn("GPU pass", ImGuiTableColumnFlags_WidthStretch, 1.6f);
            ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableHeadersRow();
            for (const GpuScope& Scope : Timer.GetScopes())
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(Scope.Name);
                ImGui::TableSetColumnIndex(1);
                RightAligned("%.2f", Scope.Ms);
                ImGui::TableSetColumnIndex(2);
                const float Share = std::clamp(Scope.Ms / GpuTotal, 0.0f, 1.0f);
                char Percent[16];
                snprintf(Percent, sizeof(Percent), "%.0f%%", Share * 100.0f);
                ImGui::ProgressBar(Share, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight()),
                                   Percent);
            }
            ImGui::EndTable();
        }
        ImGui::PopFont();

        const VkExtent2D Extent = VK.GetRenderExtent();
        float TraceMs = 0.0f;
        for (const GpuScope& Scope : Timer.GetScopes())
        {
            TraceMs = Scope.Name == std::string("Path trace") ? Scope.Ms : TraceMs;
        }
        ImGui::Spacing();
        if (TraceMs > 0.0f)
        {
            ImGui::TextDisabled("Throughput");
            ImGui::SameLine();
            ImGui::Text("%.1f M paths/s at %u x %u",
                        static_cast<float>(Extent.width) * Extent.height / (TraceMs * 1000.0f),
                        Extent.width, Extent.height);
        }

        ImGui::TableSetColumnIndex(2);
        const FrameStats::Summary Frame = Stats.Summarize(Stats.FrameMs);
        const FrameStats::Summary Gpu = Stats.Summarize(Stats.GpuMs);
        const float Ceiling = std::max(Frame.Hi * 1.15f, 1.0f);
        ImGui::TextDisabled("Frame and GPU time, last %d frames", Stats.Count);
        const float GraphWidth = ImGui::GetContentRegionAvail().x;
        const float GraphHeight =
            std::max((ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y) * 0.5f,
                     kGraphMinHeight);
        char Overlay[64];
        snprintf(Overlay, sizeof(Overlay), "frame %.2f ms", Frame.Avg);
        ImGui::PlotLines("##FrameGraph", Stats.FrameMs, FrameStats::kFrameCount, Stats.Next,
                         Overlay, 0.0f, Ceiling, ImVec2(GraphWidth, GraphHeight));
        snprintf(Overlay, sizeof(Overlay), "gpu %.2f ms", Gpu.Avg);
        ImGui::PushStyleColor(ImGuiCol_PlotLines, kWarn);
        ImGui::PlotLines("##GpuGraph", Stats.GpuMs, FrameStats::kFrameCount, Stats.Next,
                         Overlay, 0.0f, Ceiling, ImVec2(GraphWidth, GraphHeight));
        ImGui::PopStyleColor();
        ImGui::EndTable();
    }
    ImGui::End();
}

void Gui::DrawConsole()
{
    if (!ImGui::Begin("Console", &ShowConsole))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear"))
    {
        Messages.Clear();
    }
    ImGui::SameLine();
    ConsoleFilter.Draw("##ConsoleFilter", -FLT_MIN);
    if (!ConsoleFilter.IsActive() && !ImGui::IsItemActive())
    {
        const ImVec2 Min = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(Min.x + ImGui::GetStyle().FramePadding.x,
                   Min.y + ImGui::GetStyle().FramePadding.y),
            ImGui::GetColorU32(kTextDim), "Filter messages");
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.095f, 0.095f, 0.10f, 1.0f));
    if (ImGui::BeginChild("ConsoleLines", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::PushFont(Fonts.Mono);
        for (const LogEntry& Entry : Messages.GetEntries())
        {
            if (!ConsoleFilter.PassFilter(Entry.Text.c_str()))
            {
                continue;
            }
            const int Minutes = static_cast<int>(Entry.Time / 60.0);
            const double Seconds = Entry.Time - Minutes * 60.0;
            ImGui::TextColored(kTextDim, "%02d:%06.3f", Minutes, Seconds);
            ImGui::SameLine();
            const ImVec4 Color = Entry.Level == LogLevel::kError     ? kError
                                 : Entry.Level == LogLevel::kWarning ? kWarn
                                                                     : ImVec4(0.85f, 0.86f, 0.87f, 1.0f);
            ImGui::TextColored(Color, "%s", Entry.Text.c_str());
        }
        if (Messages.GetAddCount() != ConsoleSeen)
        {
            ConsoleSeen = Messages.GetAddCount();
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::PopFont();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
}

}  // namespace rtrt
