#pragma once

#include <SDL3/SDL.h>

// ---------------------------------------------------------------------------
// SDLInputMapper.h
//
// Maps SDL input events to simulation timescale controls and application
// shutdown requests.
//
// Key bindings:
//   Space -> Toggle pause (timescale 0.0) vs last active speed (1.0 default)
//   1     -> 1.0x speed
//   2     -> 2.0x speed
//   8     -> 8.0x speed
//   Esc   -> Request shutdown
//
// ImGui capture safety:
//   If ImGui is capturing input (e.g. typing in a debug console field),
//   the input mapper ignores these keys so we don't trigger speed changes
//   or close the game accidentally.
// ---------------------------------------------------------------------------

namespace dt
{
    class Application;

    class SDLInputMapper
    {
    public:
        SDLInputMapper() = default;

        // Process a single SDL_Event. Returns false if a shutdown was requested.
        bool ProcessEvent(const SDL_Event& ev, Application& app);

    private:
        float m_lastActiveTimeScale = 1.0f;
    };
}
