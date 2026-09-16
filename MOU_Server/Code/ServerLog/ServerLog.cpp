#include "ServerLog/ServerLog.h"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
namespace MOU::ServerLog
{
void Initialize()
{
#ifdef _WIN32
    ::SetConsoleOutputCP(CP_UTF8);
#endif
    ::setvbuf(stdout, nullptr, _IONBF, 0);
}
void Print(const char* Format, ...)
{
    // 기존 printf 인자를 먼저 완성된 문자열로 만든다. 개행마다 시각을 붙이기 위해서다.
    va_list Args;
    va_start(Args, Format);
    va_list SizeArgs;
    va_copy(SizeArgs, Args);
    const int Length = std::vsnprintf(nullptr, 0, Format, SizeArgs);
    va_end(SizeArgs);
    if (Length < 0)
    {
        va_end(Args);
        return;
    }
    std::vector<char> Message(static_cast<size_t>(Length) + 1);
    std::vsnprintf(Message.data(), Message.size(), Format, Args);
    va_end(Args);

    // 줄 시작 상태도 같은 락으로 보호한다. 여러 Print 호출로 나뉜 줄에는
    // 시각을 중복해서 붙이지 않고, 한 호출의 출력이 다른 스레드와 섞이지 않게 한다.
    static std::mutex OutputMutex;
    static bool AtLineStart = true;
    std::lock_guard<std::mutex> Lock(OutputMutex);
    const std::time_t Now = std::time(nullptr);
    std::tm LocalTime{};
#ifdef _WIN32
    const bool HasTime = ::localtime_s(&LocalTime, &Now) == 0;
#else
    const bool HasTime = ::localtime_r(&Now, &LocalTime) != nullptr;
#endif
    char Timestamp[80] = {};
    if (HasTime)
    {
        std::snprintf(Timestamp, sizeof(Timestamp),
            "[%04d/%02d/%02d/%02d시%02d분%02d초] ",
            LocalTime.tm_year + 1900, LocalTime.tm_mon + 1, LocalTime.tm_mday,
            LocalTime.tm_hour, LocalTime.tm_min, LocalTime.tm_sec);
    }
    else
    {
        std::snprintf(Timestamp, sizeof(Timestamp), "[시각 확인 실패] ");
    }
    for (int Start = 0; Start < Length;)
    {
        int End = Start;
        while (End < Length && Message[End] != '\n') ++End;
        if (AtLineStart && End > Start) std::fputs(Timestamp, stdout);
        if (End < Length) ++End; // 개행까지 한 번에 출력한다.
        std::fwrite(Message.data() + Start, 1, static_cast<size_t>(End - Start), stdout);
        AtLineStart = Message[End - 1] == '\n';
        Start = End;
    }
}
}
