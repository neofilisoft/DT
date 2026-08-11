#include "core/logging/Logger.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>

namespace dt
{
    const char* ToString(LogLevel level)
    {
        switch (level)
        {
            case LogLevel::Trace:   return "TRACE";
            case LogLevel::Info:    return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error:   return "ERROR";
            case LogLevel::Fatal:   return "FATAL";
            default:                return "?????";
        }
    }

    const char* ToString(LogCategory category)
    {
        switch (category)
        {
            case LogCategory::Core:          return "Core";
            case LogCategory::Memory:        return "Memory";
            case LogCategory::Jobs:          return "Jobs";
            case LogCategory::Reflection:    return "Reflection";
            case LogCategory::Serialization: return "Serialization";
            case LogCategory::FileSystem:    return "FileSystem";
            case LogCategory::Config:        return "Config";
            case LogCategory::Simulation:    return "Simulation";
            case LogCategory::Navigation:    return "Navigation";
            case LogCategory::AI:            return "AI";
            case LogCategory::Asset:         return "Asset";
            case LogCategory::Renderer:      return "Renderer";
            case LogCategory::Audio:         return "Audio";
            case LogCategory::Physics:       return "Physics";
            case LogCategory::Scripting:     return "Scripting";
            case LogCategory::Editor:        return "Editor";
            default:                         return "Invalid";
        }
    }

    // --- ConsoleLogSink ------------------------------------------------------

    void ConsoleLogSink::Write(const LogMessage& message)
    {
        std::FILE* stream = (message.level >= LogLevel::Error) ? stderr : stdout;
        std::fprintf(stream, "[%s][%s] %s\n",
            ToString(message.level), ToString(message.category), message.text.c_str());
    }

    void ConsoleLogSink::Flush()
    {
        std::fflush(stdout);
        std::fflush(stderr);
    }

    // --- FileLogSink -----------------------------------------------------------

    struct FileLogSink::Impl
    {
        std::ofstream stream;
    };

    FileLogSink::FileLogSink(const std::string& path)
        : m_impl(std::make_unique<Impl>())
    {
        m_impl->stream.open(path, std::ios::out | std::ios::app);
    }

    FileLogSink::~FileLogSink() = default;

    void FileLogSink::Write(const LogMessage& message)
    {
        if (!m_impl->stream.is_open())
        {
            return;
        }

        const auto now = std::chrono::system_clock::now();
        const auto nowTimeT = std::chrono::system_clock::to_time_t(now);

        char timeBuf[32];
        std::tm tmBuf{};
#if defined(DT_PLATFORM_WINDOWS)
        localtime_s(&tmBuf, &nowTimeT);
#else
        localtime_r(&nowTimeT, &tmBuf);
#endif
        std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tmBuf);

        m_impl->stream << '[' << timeBuf << "][" << ToString(message.level) << "]["
                       << ToString(message.category) << "] " << message.text << '\n';
    }

    void FileLogSink::Flush()
    {
        m_impl->stream.flush();
    }

    // --- Logger --------------------------------------------------------------

    Logger& Logger::Get()
    {
        static Logger instance;
        return instance;
    }

    Logger::Logger()
    {
        // Default minimum level is Info everywhere; Trace-level logging is
        // opt-in per category (typically toggled from the Editor's log
        // panel or a config file) since Trace volume across 15+ categories
        // running every simulation tick would otherwise drown the console.
        for (usize i = 0; i < static_cast<usize>(LogCategory::Count); ++i)
        {
            m_minLevels[i] = LogLevel::Info;
        }
    }

    void Logger::AddSink(std::unique_ptr<ILogSink> sink)
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_sinks.push_back(std::move(sink));
    }

    void Logger::SetMinLevel(LogCategory category, LogLevel level)
    {
        m_minLevels[static_cast<usize>(category)] = level;
    }

    LogLevel Logger::GetMinLevel(LogCategory category) const
    {
        return m_minLevels[static_cast<usize>(category)];
    }

    void Logger::Log(LogLevel level, LogCategory category, std::string text)
    {
        LogMessage message{
            level,
            category,
            std::move(text),
            static_cast<u64>(std::hash<std::thread::id>{}(std::this_thread::get_id()))
        };

        std::lock_guard<std::mutex> lock(m_sinkMutex);
        for (auto& sink : m_sinks)
        {
            sink->Write(message);
        }

        if (level == LogLevel::Fatal)
        {
            for (auto& sink : m_sinks)
            {
                sink->Flush();
            }
        }
    }
}
