#pragma once

#include "core/platform/Types.h"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// SDLWindow.h
//
// RAII wrapper around SDL3 window and OS event polling.
//
// In DTEngine, the platform layer is bootstrapped via SDL3.
// The event pump runs on the render thread (due to OS limitations requiring
// windows to be managed on the thread that created them).
//
// This wrapper handles:
//   - SDL video subsystem initialization.
//   - OS window creation with SDL_WINDOW_VULKAN flags.
//   - Vulkan surface creation wrapper.
//   - SDL event loop polling.
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class SDLWindow
    {
    public:
        SDLWindow() = default;
        ~SDLWindow();

        SDLWindow(const SDLWindow&)            = delete;
        SDLWindow& operator=(const SDLWindow&) = delete;
        SDLWindow(const SDLWindow&&)           = delete;
        SDLWindow& operator=(SDLWindow&&)      = delete;

        bool Initialize(const std::string& title, u32 width, u32 height);
        void Shutdown();

        // Gets the list of instance extensions required by Vulkan to create
        // a surface on this window on the current OS.
        std::vector<const char*> GetRequiredInstanceExtensions() const;

        // Creates the VkSurfaceKHR for this window.
        VkSurfaceKHR CreateSurface(VkInstance instance);

        // Pumps OS window events. Returns false if a close request was received.
        // On resize events, outWidth and outHeight are populated and outResized is set to true.
        bool PollEvents(bool& outResized, u32& outWidth, u32& outHeight);

        SDL_Window* Handle() const { return m_window; }
        bool IsInitialized() const { return m_window != nullptr; }

    private:
        SDL_Window* m_window = nullptr;
        bool        m_sdlInitialized = false;
    };
}
