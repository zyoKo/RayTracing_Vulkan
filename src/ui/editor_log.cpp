#include "ui/editor_log.h"

#include <cstdarg>
#include <cstdio>
#include <string>

namespace rtrt
{

void EditorLog::Add(double Time, LogLevel Level, const char* Format, ...)
{
    char Buffer[512];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Buffer, sizeof(Buffer), Format, Args);
    va_end(Args);

    Entries.push_back({Time, Level, Buffer});
    if (Entries.size() > kMaxEntries)
    {
        Entries.pop_front();
    }
    AddCount++;
}

}  // namespace rtrt
