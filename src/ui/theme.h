#ifndef RTRT_SRC_UI_THEME_H_
#define RTRT_SRC_UI_THEME_H_

#include <imgui.h>

namespace rtrt
{

// The fonts the editor draws with; any can fall back to ImGui's default font.
struct EditorFonts
{
    ImFont* Ui = nullptr;
    ImFont* Bold = nullptr;
    ImFont* Title = nullptr;
    ImFont* Mono = nullptr;
};

// The accent used for selected, active and checked states.
inline constexpr ImVec4 kAccent{0.24f, 0.70f, 0.72f, 1.0f};
inline constexpr ImVec4 kTextDim{0.58f, 0.60f, 0.62f, 1.0f};
inline constexpr ImVec4 kGood{0.45f, 0.80f, 0.45f, 1.0f};
inline constexpr ImVec4 kWarn{0.95f, 0.72f, 0.30f, 1.0f};
inline constexpr ImVec4 kError{0.95f, 0.40f, 0.38f, 1.0f};

// Adds Segoe UI and Consolas from the Windows font folder when they exist.
EditorFonts LoadEditorFonts();

// Dark charcoal editor colours with compact spacing.
void ApplyEditorTheme();

}  // namespace rtrt

#endif  // RTRT_SRC_UI_THEME_H_
