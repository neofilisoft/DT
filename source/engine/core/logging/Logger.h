#pragma once

#include "core/platform/Types.h"

#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------
// Logger.h
//
// Categorized, leveled logging with pluggable sinks (console, file, and
// later an in-editor log panel sink). Uses std::format (C++20) for
// compile-time format-string checking rather than printf-style varargs -
// a malformed DT_LOG_INFO("{}", ) call is a compile error, not a runtime
// crash or garbage output discovered during a play session.
//
// Threading: LogSink::Write is called from any thread (the job system runs
// gameplay code across N workers, and any of them may log). Logger itself
// serializes sink writes under a single mutex. This is intentionally not
// lock-free: logging is not a per-frame-critical hot path, and correctness
// (no interleaved/garbled log lines from concurrent writers) matters more
// here than throughput.
// ---------------------------------------------------------------------------

namespace dt
{
    enum class LogLevel : u8
    {
        Trace = 0,
        Info,
        Warning,
        Error,
        Fatal
    };

    enum class LogCategory : u8
    {
        Core = 0,
        Memory,
        Jobs,
        Reflection,
        Serialization,
        FileSystem,
        Config,
        Simulation,
        Navigation,
        AI,
        Asset,
        Renderer,
        Audio,
        Physics,
        Scripting,
        Editor,
        Count
    };

    const char* ToString(LogLevel level);
    const char* ToString(LogCategory category);

    struct LogMessage
    {
        LogLevel level;
        LogCategory category;
        std::string text;
        u64 threadId;
    };

    class ILogSink
    {
    public:
        virtual ~ILogSink() = default;
        virtual void Write(const LogMessage& message) = 0;
        virtual void Flush() {}
    };

    class ConsoleLogSink final : public ILogSink
    {
    public:
        void Write(const LogMessage& message) override;
        void Flush() override;
    };

    class FileLogSink final : public ILogSink
    {
    public:
        explicit FileLogSink(const std::string& path);
        ~FileLogSink() override;

        void Write(const LogMessage& message) override;
        void Flush() override;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

    class Logger
    {
    public:
        static Logger& Get();

        void AddSink(std::unique_ptr<ILogSink> sink);

        // Per-category minimum level. Anything below this level for a given
        // category is dropped before formatting even runs, so disabling
        // Trace-level Navigation logs at runtime costs a single integer
        // comparison, not a discarded std::format allocation.
        void SetMinLevel(LogCategory category, LogLevel level);
        LogLevel GetMinLevel(LogCategory category) const;

        void Log(LogLevel level, LogCategory category, std::string text);

    private:
        Logger();

        std::mutex m_sinkMutex;
        std::vector<std::unique_ptr<ILogSink>> m_sinks;
        LogLevel m_minLevels[static_cast<usize>(LogCategory::Count)];
    };

    namespace detail
    {
        inline u64 CurrentThreadIdHash();
    }
}

// ---------------------------------------------------------------------------
// Logging macros. Format-string checking happens at the call site via
// std::format, so a type mismatch between the format string and arguments
// is a compile error.
// ---------------------------------------------------------------------------

#define DT_LOG_TRACE(category, fmt, ...)                                                              \
    do {                                                                                               \
        if (::dt::Logger::Get().GetMinLevel(category) <= ::dt::LogLevel::Trace)                       \
            ::dt::Logger::Get().Log(::dt::LogLevel::Trace, category, std::format(fmt, ##__VA_ARGS__)); \
    } while (0)

#define DT_LOG_INFO(category, fmt, ...)                                                               \
    do {                                                                                               \
        if (::dt::Logger::Get().GetMinLevel(category) <= ::dt::LogLevel::Info)                        \
            ::dt::Logger::Get().Log(::dt::LogLevel::Info, category, std::format(fmt, ##__VA_ARGS__));  \
    } while (0)

#define DT_LOG_WARN(category, fmt, ...)                                                                \
    do {                                                                                                \
        if (::dt::Logger::Get().GetMinLevel(category) <= ::dt::LogLevel::Warning)                     \
            ::dt::Logger::Get().Log(::dt::LogLevel::Warning, category, std::format(fmt, ##__VA_ARGS__)); \
    } while (0)

#define DT_LOG_ERROR(category, fmt, ...)                                                               \
    do {                                                                                                \
        if (::dt::Logger::Get().GetMinLevel(category) <= ::dt::LogLevel::Error)                        \
            ::dt::Logger::Get().Log(::dt::LogLevel::Error, category, std::format(fmt, ##__VA_ARGS__));  \
    } while (0)

#define DT_LOG_FATAL(category, fmt, ...)                                                               \
    do {                                                                                                \
        ::dt::Logger::Get().Log(::dt::LogLevel::Fatal, category, std::format(fmt, ##__VA_ARGS__));      \
    } while (0)
