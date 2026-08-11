#include "renderer/sdl/SDLWindow.h"

#include "core/logging/Logger.h"
#include "core/platform/Assert.h"

#include <SDL3/SDL_vulkan.h>
#include <imgui.h>

extern bool ImGui_ImplSDL3_ProcessEvent(const SDL_Event* event);

namespace dt::renderer
{
    SDLWindow::~SDLWindow()
    {
        Shutdown();
    }

    bool SDLWindow::Initialize(const std::string& title, u32 width, u32 height)
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            DT_LOG_ERROR(LogCategory::Renderer, "SDLWindow: failed to initialize SDL video subsystem: {}", SDL_GetError());
            return false;
        }
        m_sdlInitialized = true;

        // Force Vulkan library load before creating window
        if (!SDL_Vulkan_LoadLibrary(nullptr))
        {
            DT_LOG_ERROR(LogCategory::Renderer, "SDLWindow: failed to load Vulkan library via SDL3: {}", SDL_GetError());
            SDL_Quit();
            m_sdlInitialized = false;
            return false;
        }

        m_window = SDL_CreateWindow(
            title.c_str(),
            static_cast<int>(width),
            static_cast<int>(height),
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
        );

        if (m_window == nullptr)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "SDLWindow: failed to create SDL window: {}", SDL_GetError());
            SDL_Vulkan_UnloadLibrary();
            SDL_Quit();
            m_sdlInitialized = false;
            return false;
        }

        DT_LOG_INFO(LogCategory::Renderer, "SDLWindow: created successfully ({}x{})", width, height);
        return true;
    }

    void SDLWindow::Shutdown()
    {
        if (m_window != nullptr)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        if (m_sdlInitialized)
        {
            SDL_Vulkan_UnloadLibrary();
            SDL_Quit();
            m_sdlInitialized = false;
        }
    }

    std::vector<const char*> SDLWindow::GetRequiredInstanceExtensions() const
    {
        u32 count = 0;
        const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
        if (extensions == nullptr || count == 0)
        {
            DT_LOG_WARN(LogCategory::Renderer, "SDLWindow: failed to get required instance extensions from SDL: {}", SDL_GetError());
            return {};
        }

        std::vector<const char*> result(count);
        for (u32 i = 0; i < count; ++i)
        {
            result[i] = extensions[i];
        }
        return result;
    }

    VkSurfaceKHR SDLWindow::CreateSurface(VkInstance instance)
    {
        DT_ASSERT(m_window != nullptr, "SDLWindow::CreateSurface: window is not initialized");
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(m_window, instance, nullptr, &surface))
        {
            DT_LOG_ERROR(LogCategory::Renderer, "SDLWindow: failed to create Vulkan surface: {}", SDL_GetError());
            return VK_NULL_HANDLE;
        }
        return surface;
    }

    bool SDLWindow::PollEvents(bool& outResized, u32& outWidth, u32& outHeight)
    {
        outResized = false;
        SDL_Event ev;
        bool keepRunning = true;

        while (SDL_PollEvent(&ev))
        {
            // First forward events to ImGui if it is initialized
            ImGuiIO* io = ImGui::GetCurrentContext() ? &ImGui::GetIO() : nullptr;
            if (io != nullptr)
            {
                // We'll let ImGui's SDL3 backend process the event.
                // ImGui_ImplSDL3_ProcessEvent will be called from our renderer wrapper,
                // but we also call it here to ensure it intercepts input.
                ImGui_ImplSDL3_ProcessEvent(&ev);
            }

            if (ev.type == SDL_EVENT_QUIT)
            {
                keepRunning = false;
            }
            else if (ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                outResized = true;
                outWidth = static_cast<u32>(ev.window.data1);
                outHeight = static_cast<u32>(ev.window.data2);
                DT_LOG_INFO(LogCategory::Renderer, "SDLWindow: window resized to {}x{}", outWidth, outHeight);
            }
            else if (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                keepRunning = false;
            }
        }

        return keepRunning;
    }
}
