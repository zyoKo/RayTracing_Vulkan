#ifndef RTRT_SRC_UI_PROPERTY_GRID_H_
#define RTRT_SRC_UI_PROPERTY_GRID_H_

#include <imgui.h>

namespace rtrt
{

// A two-column table: labels on the left, full-width widgets on the right. Between
// BeginProperties and EndProperties, call PropertyRow before each widget and give the
// widget a hidden "##" label.
bool BeginProperties(const char* Id);
void EndProperties();

// Starts a row with Label, and an optional hover tooltip, and leaves the cursor in the
// value column with the next item stretched to its width.
void PropertyRow(const char* Label, const char* Tooltip = nullptr);

// A read-only row with formatted text in the value column.
void PropertyText(const char* Label, const char* Format, ...);

// A collapsing section header drawn in HeaderFont; true while it's open.
bool Section(ImFont* HeaderFont, const char* Label, bool DefaultOpen = true);

// A small "(?)" marker with a tooltip, on the same line as the last item.
void HelpMarker(const char* Text);

}  // namespace rtrt

#endif  // RTRT_SRC_UI_PROPERTY_GRID_H_
