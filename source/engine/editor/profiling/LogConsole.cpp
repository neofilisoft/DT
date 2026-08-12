#include "editor/profiling/LogConsole.h"
#include "editor/core/EditorContext.h"

#include <imgui.h>
#include <cstring>
#include <memory>

namespace dt::editor
{
    class ProxySink : public dt::ILogSink
    {
    public:
        ProxySink(LogConsole* console) : m_console(console) {}
        void Write(const dt::LogMessage& message) override {
            if (m_console) m_console->Write(message);
        }
        void Detach() { m_console = nullptr; }
    private:
        LogConsole* m_console;
    };

    static ProxySink* s_proxySink = nullptr;

    LogConsole::LogConsole()
        : EditorPanel("Log Console")
    {
    }

    void LogConsole::Init(EditorContext& ctx)
    {
        (void)ctx;
        auto proxy = std::make_unique<ProxySink>(this);
        s_proxySink = proxy.get();
        dt::Logger::Get().AddSink(std::move(proxy));
    }

    void LogConsole::Shutdown()
    {
        if (s_proxySink) s_proxySink->Detach();
    }

    void LogConsole::Write(const dt::LogMessage& message)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.push_back({ message.text, message.level });
        if (m_entries.size() > 4096)
            m_entries.erase(m_entries.begin(), m_entries.begin() + 1024);
    }

    void LogConsole::Construct(EditorContext& ctx)
    {
        (void)ctx;
        ImGui::Begin("Log Console", &m_isOpen);

        // Toolbar
        ImGui::Checkbox("Info",  &m_showInfo);  ImGui::SameLine();
        ImGui::Checkbox("Warn",  &m_showWarn);  ImGui::SameLine();
        ImGui::Checkbox("Error", &m_showError); ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll); ImGui::SameLine();
        if (ImGui::SmallButton("Clear"))
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_entries.clear();
        }

        ImGui::Separator();

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##filter", "Filter...", m_filterBuf, sizeof(m_filterBuf));

        ImGui::Separator();

        ImGui::BeginChild("##logscroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        std::string filterStr = m_filterBuf;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& entry : m_entries)
            {
                if (!m_showInfo  && entry.level == dt::LogLevel::Info)  continue;
                if (!m_showWarn  && entry.level == dt::LogLevel::Warning) continue;
                if (!m_showError && entry.level == dt::LogLevel::Error) continue;
                if (!filterStr.empty() && entry.text.find(filterStr) == std::string::npos) continue;

                ImVec4 color;
                switch (entry.level)
                {
                    case dt::LogLevel::Warning: color = ImVec4(1.0f, 0.9f, 0.1f, 1.0f); break;
                    case dt::LogLevel::Error:   color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break;
                    default:                color = ImVec4(0.9f, 0.9f, 0.9f, 1.0f); break;
                }
                ImGui::TextColored(color, "%s", entry.text.c_str());
            }
        }

        if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::End();
    }
}
