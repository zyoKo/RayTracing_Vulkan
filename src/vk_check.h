#ifndef RTRT_SRC_VK_CHECK_H_
#define RTRT_SRC_VK_CHECK_H_

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

namespace rtrt
{

// Prints "File:Line: " and the printf-style message, then aborts.
[[noreturn]] inline void FatalError(const char* File, int Line, const char* Format, ...)
{
    std::fprintf(stderr, "%s:%d: ", File, Line);
    va_list Args;
    va_start(Args, Format);
    std::vfprintf(stderr, Format, Args);
    va_end(Args);
    std::fputc('\n', stderr);
    std::fflush(stderr);
#ifdef _MSC_VER
    // Without this, Windows shows its crash dialog and the process waits on it.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    std::abort();
}

// Success codes such as VK_INCOMPLETE and VK_SUBOPTIMAL_KHR are positive and pass.
inline void CheckVkResult(VkResult Result, const char* Call, const char* File, int Line)
{
    if (Result < 0)
    {
        FatalError(File, Line, "%s failed: %s", Call, string_VkResult(Result));
    }
}

}  // namespace rtrt

#define RTRT_CHECK_VK(call) ::rtrt::CheckVkResult((call), #call, __FILE__, __LINE__)

#define RTRT_FATAL(...) ::rtrt::FatalError(__FILE__, __LINE__, __VA_ARGS__)

#endif  // RTRT_SRC_VK_CHECK_H_
