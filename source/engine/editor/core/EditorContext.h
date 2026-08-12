#pragma once
// ---------------------------------------------------------------------------
// editor/core/EditorContext.h
//
// Global editor state shared across all panels.
// Inspired by Lacrima's EditorContext / g_editor global.
// Holds: selection, play state, undo stack reference.
// ---------------------------------------------------------------------------

#include "runtime/Entity.h"

#include <functional>
#include <string>
#include <vector>

namespace dt::sim  { class SimulationWorld; }
namespace dt::renderer { class VulkanContext; }

namespace dt::editor
{
    // Mode the editor is currently in.
    enum class EditorMode
    {
        Edit,        // Scene editing - simulation is paused.
        Play,        // PIE (Play-In-Editor) - simulation is running.
        Paused,      // PIE but user pressed the pause button.
    };

    // Simple undo/redo command abstraction.
    struct EditorCommand
    {
        std::string                  description;
        std::function<void()>        execute;
        std::function<void()>        undo;
    };

    class EditorContext
    {
    public:
        EditorContext() = default;

        // ----- Selection -----
        bool HasSelection() const      { return !m_selectedEntity.IsNull(); }
        dt::Entity SelectedEntity() const { return m_selectedEntity; }
        void Select(dt::Entity e)      { m_selectedEntity = e; }
        void ClearSelection()          { m_selectedEntity = dt::Entity{}; }

        // ----- Mode -----
        EditorMode Mode() const        { return m_mode; }
        void       SetMode(EditorMode m) { m_mode = m; }
        bool       IsPlaying() const   { return m_mode == EditorMode::Play; }

        // ----- World reference -----
        // Set by EditorApp after SimulationWorld is created.
        void SetWorld(dt::sim::SimulationWorld* world) { m_world = world; }
        dt::sim::SimulationWorld* World() const { return m_world; }

        // ----- Undo/Redo -----
        void ExecuteCommand(EditorCommand cmd);
        void Undo();
        void Redo();
        bool CanUndo() const { return !m_undoStack.empty(); }
        bool CanRedo() const { return !m_redoStack.empty(); }

        // ----- Viewport fullscreen toggle (ala Lacrima) -----
        bool IsViewportFullscreen() const { return m_viewportFullscreen; }
        void ToggleViewportFullscreen() { m_viewportFullscreen = !m_viewportFullscreen; }

    private:
        dt::Entity                     m_selectedEntity{};
        EditorMode                     m_mode = EditorMode::Edit;
        dt::sim::SimulationWorld*      m_world = nullptr;
        bool                           m_viewportFullscreen = false;

        std::vector<EditorCommand>     m_undoStack;
        std::vector<EditorCommand>     m_redoStack;
    };
}
