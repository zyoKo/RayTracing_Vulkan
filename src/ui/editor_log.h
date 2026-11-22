#ifndef RTRT_SRC_UI_EDITOR_LOG_H_
#define RTRT_SRC_UI_EDITOR_LOG_H_

#include <cstddef>
#include <deque>
#include <string>

namespace rtrt
{

enum class LogLevel
{
    kInfo,
    kWarning,
    kError,
};

struct LogEntry
{
    // Seconds since the editor started
    double Time = 0.0;
    LogLevel Level = LogLevel::kInfo;
    std::string Text;
};

// The editor console's messages, oldest first, capped at kMaxEntries.
class EditorLog
{
public:
    static constexpr size_t kMaxEntries = 500;

    // printf-style; Time is seconds since start.
    void Add(double Time, LogLevel Level, const char* Format, ...);

    void Clear()
    {
        Entries.clear();
    }

    const std::deque<LogEntry>& GetEntries() const
    {
        return Entries;
    }

    // Goes up with every Add, so the console can scroll to new messages.
    size_t GetAddCount() const
    {
        return AddCount;
    }

private:
    std::deque<LogEntry> Entries;
    size_t AddCount = 0;
};

}  // namespace rtrt

#endif  // RTRT_SRC_UI_EDITOR_LOG_H_
