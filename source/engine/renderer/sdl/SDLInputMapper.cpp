#include "renderer/sdl/SDLInputMapper.h"

#include "core/logging/Logger.h"
#include "runtime/Application.h"

#include <imgui.h>

namespace dt
{
    bool SDLInputMapper::ProcessEvent(const SDL_Event& ev, Application& app)
    {
        // Safety check: if ImGui is currently capturing the keyboard (e.g. user
        // is typing in an input field), ignore all gameplay hotkeys.
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard)
        {
            return true;
        }

        if (ev.type == SDL_EVENT_KEY_DOWN)
        {
            const SDL_Keycode key = ev.key.key;

            if (key == SDLK_ESCAPE)
            {
                DT_LOG_INFO(LogCategory::Core, "SDLInputMapper: Esc pressed - requesting shutdown");
                app.RequestShutdown();
                return false;
            }
            else if (key == SDLK_SPACE)
            {
                const float currentScale = app.Sim().GetTimeScale();
                if (currentScale > 0.0f)
                {
                    m_lastActiveTimeScale = currentScale;
                    app.Sim().SetTimeScale(0.0f);
                    DT_LOG_INFO(LogCategory::Simulation, "SDLInputMapper: Space pressed - simulation paused");
                }
                else
                {
                    app.Sim().SetTimeScale(m_lastActiveTimeScale);
                    DT_LOG_INFO(LogCategory::Simulation, "SDLInputMapper: Space pressed - simulation resumed to {:.1f}x", m_lastActiveTimeScale);
                }
            }
            else if (key == SDLK_1)
            {
                app.Sim().SetTimeScale(1.0f);
                m_lastActiveTimeScale = 1.0f;
                DT_LOG_INFO(LogCategory::Simulation, "SDLInputMapper: '1' pressed - simulation set to 1x");
            }
            else if (key == SDLK_2)
            {
                app.Sim().SetTimeScale(2.0f);
                m_lastActiveTimeScale = 2.0f;
                DT_LOG_INFO(LogCategory::Simulation, "SDLInputMapper: '2' pressed - simulation set to 2x");
            }
            else if (key == SDLK_8)
            {
                app.Sim().SetTimeScale(8.0f);
                m_lastActiveTimeScale = 8.0f;
                DT_LOG_INFO(LogCategory::Simulation, "SDLInputMapper: '8' pressed - simulation set to 8x");
            }
        }

        return true;
    }
}
