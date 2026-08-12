#pragma once
// ---------------------------------------------------------------------------
// editor/profiling/LogConsole.h
//
// An in-editor console panel that captures Logger outputs and lets the
// developer filter by log level and category.
// ---------------------------------------------------------------------------

#include "editor/core/EditorPanel.h"
#include "core/logging/Logger.h"

#include <string>
#include <vector>
#include <mutex>

namespace dt::editor
{
    struct LogEntry
    {
        std::string    text;
        dt::LogLevel   level = dt::LogLevel::Info;
    };

    class LogConsole final : public EditorPanel, public dt::ILogSink
    {
    public:
        LogConsole();

        void Init(EditorContext& ctx) override;
        void Construct(EditorContext& ctx) override;
        void Shutdown() override;

        // dt::ILogSink
        void Write(const dt::LogMessage& message) override;

    private:
        std::vector<LogEntry>   m_entries;
        std::mutex              m_mutex;
        bool                    m_autoScroll = true;
        bool                    m_showInfo   = true;
        bool                    m_showWarn   = true;
        bool                    m_showError  = true;
        char                    m_filterBuf[128] = {};
    };
}
