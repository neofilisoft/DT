#pragma once

#include "core/platform/Types.h"
#include "runtime/SimulationSnapshot.h"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

// ---------------------------------------------------------------------------
// GameUILayer.h
//
// Game-facing HUD rendered using Dear ImGui, intentionally separate from
// ImGuiLayer (the engine debug overlay).
//
// Separation rationale:
//   ImGuiLayer    -> debug/editor windows: tick rate, sim inspector, profiler.
//                    Drawn with ImGui's default dark developer style.
//   GameUILayer   -> in-game HUD: needs bars, entity status, day/time display.
//                    Drawn with a premium custom style that feels like part of
//                    the game, not a developer tool.
//
// Both layers share the same ImGui context and Vulkan descriptor pool (owned
// by ImGuiLayer). GameUILayer only defines its own draw calls within that
// shared context. Draw order: Scene -> GameHUD -> DebugHUD.
//
// Both 2D and 3D games use the same GameUILayer because HUD elements always
// live in screen-space regardless of the scene projection mode.
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class GameUILayer
    {
    public:
        GameUILayer()  = default;
        ~GameUILayer() = default;

        GameUILayer(const GameUILayer&)            = delete;
        GameUILayer& operator=(const GameUILayer&) = delete;

        // Initialize the Game UI style. Must be called after ImGuiLayer::Initialize
        // has set up the ImGui context and Vulkan backend.
        void Initialize();

        void Shutdown();

        // Draw one frame of game HUD. Call this between ImGuiLayer::BeginFrame()
        // and ImGuiLayer::Render() so all draw calls land in the same ImGui frame.
        // `snapshot` provides the sim state to display (needs, time, selected entity).
        // `screenW` / `screenH` are the current framebuffer dimensions for
        // computing normalized layout positions.
        void DrawGameHUD(const SimSnapshot& snapshot, u32 screenW, u32 screenH);

        bool IsInitialized() const { return m_initialized; }

    private:
        void PushGameStyle();
        void PopGameStyle();

        // HUD sub-panels
        void DrawDayTimeBar(const SimSnapshot& snapshot, u32 screenW);
        void DrawEntityStatusPanel(const SimSnapshot& snapshot, u32 screenH);
        void DrawSelectedEntityNeeds(const SimSnapshot& snapshot, u32 screenW, u32 screenH);

        bool m_initialized  = false;
        int  m_selectedIdx  = 0;       // Which entity the player has selected (index into snapshot.proxies)
    };
}
