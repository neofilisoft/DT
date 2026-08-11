#include "renderer/ConsoleRenderer.h"

#include "core/logging/Logger.h"

#include <cstdio>

namespace dt
{
    bool ConsoleRenderer::Initialize()
    {
        DT_LOG_INFO(LogCategory::Renderer, "ConsoleRenderer initialized (stand-in backend, no window/GPU)");
        return true;
    }

    void ConsoleRenderer::Shutdown()
    {
        DT_LOG_INFO(LogCategory::Renderer, "ConsoleRenderer shutdown after {} frames", m_framesRendered);
    }

    void ConsoleRenderer::Render(const SimSnapshot& snapshot)
    {
        ++m_framesRendered;

        // \r-based single-line HUD so the terminal doesn't scroll at 30
        // FPS - this is throwaway debug output for M1, not a real UI.
        std::printf("\r[frame %6llu] tick=%6llu simTime=%8.2fs scale=x%.0f entities=%3zu    ",
            static_cast<unsigned long long>(m_framesRendered),
            static_cast<unsigned long long>(snapshot.tickIndex),
            snapshot.simTimeSeconds,
            static_cast<double>(snapshot.timeScale),
            snapshot.proxies.size());
        std::fflush(stdout);
    }
}
