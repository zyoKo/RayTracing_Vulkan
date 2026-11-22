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
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x -
                         Width);
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
    if (!ImGui::Begin("Stats"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTable(
            "StatsLayout", 3,
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
            ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthStretch, 2.1f);
            ImGui::TableSetupColumn("Now", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Min", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();
            TimingRow(Stats, "Frame", "Wall time from one frame to the next",
                      Stats.FrameMs);
            TimingRow(Stats, "UI build", "Building the editor interface on the CPU",
                      Stats.UiMs);
            TimingRow(Stats, "Record", "Acquire, record, submit and present",
                      Stats.RecordMs);
            TimingRow(Stats, "Fence", "Blocked on the previous frame's fence",
                      Stats.GpuWaitMs);
            TimingRow(Stats, "VSync",
                      "Blocked in the window system until the display is ready (FIFO "
                      "present)",
                      Stats.DisplayWaitMs);
            TimingRow(Stats, "GPU", "First to last timestamp of the frame", Stats.GpuMs);
            ImGui::EndTable();
        }

        ImGui::TableSetColumnIndex(1);
        const GpuTimer& Timer = VK.GetGpuTimer();
        const float GpuTotal = std::max(Timer.GetFrameMs(), 0.001f);
        if (ImGui::BeginTable("Passes", 4, TableFlags))
        {
            ImGui::TableSetupColumn("GPU pass", ImGuiTableColumnFlags_WidthFixed,
                                    ImGui::CalcTextSize("Path trace ").x);
            ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthStretch, 1.1f);
            ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("##Bar", ImGuiTableColumnFlags_WidthStretch, 1.6f);
            ImGui::TableHeadersRow();
            for (const GpuScope& Scope : Timer.GetScopes())
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(Scope.Name);
                ImGui::TableSetColumnIndex(1);
                RightAligned("%.2f", Scope.Ms);
                const float Share = std::clamp(Scope.Ms / GpuTotal, 0.0f, 1.0f);
                ImGui::TableSetColumnIndex(2);
                RightAligned("%.0f", Share * 100.0f);
                ImGui::TableSetColumnIndex(3);
                const ImVec2 BarPos = ImGui::GetCursorScreenPos();
                const float BarWidth = ImGui::GetContentRegionAvail().x;
                const float BarHeight = ImGui::GetTextLineHeight() * 0.55f;
                const float BarTop =
                    BarPos.y + (ImGui::GetTextLineHeight() - BarHeight) * 0.5f;
                ImDrawList* DrawList = ImGui::GetWindowDrawList();
                DrawList->AddRectFilled(ImVec2(BarPos.x, BarTop),
                                        ImVec2(BarPos.x + BarWidth, BarTop + BarHeight),
                                        ImGui::GetColorU32(ImGuiCol_FrameBg), 2.0f);
                DrawList->AddRectFilled(
                    ImVec2(BarPos.x, BarTop),
                    ImVec2(BarPos.x + BarWidth * Share, BarTop + BarHeight),
                    ImGui::GetColorU32(ImVec4(0.72f, 0.74f, 0.76f, 1.0f)), 2.0f);
                ImGui::Dummy(ImVec2(BarWidth, ImGui::GetTextLineHeight()));
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
            ImGui::Text(
                "%.1f M paths/s at %u x %u",
                static_cast<float>(Extent.width) * Extent.height / (TraceMs * 1000.0f),
                Extent.width, Extent.height);
        }

        ImGui::TableSetColumnIndex(2);
        DrawTimeGraph(ImGui::GetContentRegionAvail());
        ImGui::EndTable();
    }
    ImGui::End();
}

void Gui::DrawTimeGraph(const ImVec2& Size) const
{
    const FrameStats::Summary Frame = Stats.Summarize(Stats.FrameMs);
    const FrameStats::Summary Gpu = Stats.Summarize(Stats.GpuMs);
    const ImU32 FrameColor = ImGui::GetColorU32(kAccent);
    const ImU32 GpuColor = IM_COL32(205, 208, 212, 255);

    // Legend
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const float LineHeight = ImGui::GetTextLineHeight();
    auto LegendItem = [&](ImU32 Color, const char* Text)
    {
        const ImVec2 Pos = ImGui::GetCursorScreenPos();
        DrawList->AddRectFilled(ImVec2(Pos.x, Pos.y + LineHeight * 0.35f),
                                ImVec2(Pos.x + 10.0f, Pos.y + LineHeight * 0.65f), Color);
        ImGui::Dummy(ImVec2(14.0f, LineHeight));
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextUnformatted(Text);
    };
    char Text[64];
    snprintf(Text, sizeof(Text), "Frame %.2f ms", Frame.Avg);
    LegendItem(FrameColor, Text);
    ImGui::SameLine(0.0f, 14.0f);
    snprintf(Text, sizeof(Text), "GPU %.2f ms", Gpu.Avg);
    LegendItem(GpuColor, Text);

    // Plot area with a millisecond scale whose top is a round number of milliseconds
    const ImVec2 Origin = ImGui::GetCursorScreenPos();
    const ImVec2 Avail(Size.x,
                       std::max(ImGui::GetContentRegionAvail().y, kGraphMinHeight));
    ImGui::PushFont(Fonts.Mono);
    const float AxisWidth = ImGui::CalcTextSize("00.0").x + 6.0f;
    const ImVec2 Min(Origin.x + AxisWidth, Origin.y + 2.0f);
    const ImVec2 Max(Origin.x + Avail.x, Origin.y + Avail.y - 2.0f);
    const float Steps[] = {1.0f, 2.0f, 2.5f, 5.0f, 10.0f, 20.0f, 25.0f, 50.0f, 100.0f};
    float Step = Steps[0];
    for (const float Candidate : Steps)
    {
        Step = Candidate;
        if (Candidate * 4.0f >= Frame.Hi * 1.1f)
        {
            break;
        }
    }
    const float Top = Step * 4.0f;
    DrawList->AddRectFilled(Min, Max, ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f);
    for (int I = 0; I <= 4; I++)
    {
        const float Y = Max.y - (Max.y - Min.y) * (static_cast<float>(I) / 4.0f);
        if (I > 0 && I < 4)
        {
            DrawList->AddLine(ImVec2(Min.x, Y), ImVec2(Max.x, Y),
                              IM_COL32(255, 255, 255, 18));
        }
        char Tick[16];
        snprintf(Tick, sizeof(Tick), Step < 1.0f ? "%.1f" : "%.0f", Step * I);
        const float TickWidth = ImGui::CalcTextSize(Tick).x;
        const float LabelY =
            std::clamp(Y - LineHeight * 0.5f, Origin.y, Max.y - LineHeight);
        DrawList->AddText(ImVec2(Min.x - TickWidth - 4.0f, LabelY),
                          ImGui::GetColorU32(kTextDim), Tick);
    }
    DrawList->AddText(ImVec2(Min.x + 5.0f, Min.y + 2.0f), ImGui::GetColorU32(kTextDim),
                      "ms");
    ImGui::PopFont();

    auto Series = [&](const float* Values, ImU32 Color)
    {
        if (Stats.Count < 2)
        {
            return;
        }
        ImVec2 Points[FrameStats::kFrameCount];
        const int Count = Stats.Count;
        const int First =
            (Stats.Next - Count + FrameStats::kFrameCount) % FrameStats::kFrameCount;
        for (int I = 0; I < Count; I++)
        {
            const float Value = Values[(First + I) % FrameStats::kFrameCount];
            const float T =
                static_cast<float>(I) / static_cast<float>(FrameStats::kFrameCount - 1);
            const float V = std::clamp(Value / Top, 0.0f, 1.0f);
            Points[I] = ImVec2(Min.x + (Max.x - Min.x) * T, Max.y - (Max.y - Min.y) * V);
        }
        DrawList->AddPolyline(Points, Count, Color, ImDrawFlags_None, 1.5f);
    };
    Series(Stats.GpuMs, GpuColor);
    Series(Stats.FrameMs, FrameColor);
    ImGui::Dummy(Avail);
}

void Gui::DrawConsole()
{
    if (!ImGui::Begin("Console"))
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
            const ImVec4 Color = Entry.Level == LogLevel::kError ? kError
                                 : Entry.Level == LogLevel::kWarning
                                     ? kWarn
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
