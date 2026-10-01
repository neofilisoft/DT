#pragma once

#include "runtime/Entity.h"
#include "editor/scene/EditorCamera.h"
#include "core/platform/Types.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lacrima::sim      { class SimulationWorld; }
namespace lacrima::renderer { class VulkanContext; }

namespace lacrima::editor
{
    enum class EditorMode
    {
        Edit,
        Play,
        Paused,
        Stepping, // Single-frame advance: advances 1 fixed tick then returns to Paused
    };

    struct EditorCommand
    {
        std::string                  description;
        std::function<void()>        execute;
        std::function<void()>        undo;
    };

    class EditorContext
    {
    public:
        EditorContext();
        ~EditorContext();

        // ----- Selection -----
        bool HasSelection() const              { return !m_selectedEntity.IsNull(); }
        lacrima::Entity SelectedEntity() const { return m_selectedEntity; }
        void Select(lacrima::Entity e)         { m_selectedEntity = e; }
        void ClearSelection()                  { m_selectedEntity = lacrima::Entity{}; }

        // ----- Mode -----
        EditorMode Mode() const                { return m_mode; }
        void       SetMode(EditorMode m)       { m_mode = m; }
        bool       IsPlaying() const           { return m_mode == EditorMode::Play; }
        bool       IsPaused()  const           { return m_mode == EditorMode::Paused; }
        bool       IsStepping() const          { return m_mode == EditorMode::Stepping; }
        bool       IsInPIE()   const           { return m_mode != EditorMode::Edit; }

        void TogglePause()
        {
            if (m_mode == EditorMode::Play)
                m_mode = EditorMode::Paused;
            else if (m_mode == EditorMode::Paused)
                m_mode = EditorMode::Play;
        }

        // ----- PIE Lifecycle (Dual-World) -----
        // Call PIEPlay() instead of SetMode(Play) to correctly clone the editor world.
        // Call PIEStop() instead of SetMode(Edit) to correctly destroy runtime world.
        // Call PIEStep() to advance exactly 1 fixed tick then re-pause.
        void PIEPlay();
        void PIEPause();
        void PIEStep();
        void PIEStop();

        // Consumed by the main simulation tick loop to decide whether to step:
        //   - Returns true when mode is Play or Stepping (one-shot); caller must call
        //     ConsumePIEStepRequest() after ticking when Stepping.
        bool ShouldTickRuntime() const { return m_mode == EditorMode::Play || m_mode == EditorMode::Stepping; }

        // Called by the application tick after it has advanced one frame in Stepping mode.
        // Switches back to Paused automatically.
        void ConsumePIEStepRequest() { if (m_mode == EditorMode::Stepping) m_mode = EditorMode::Paused; }

        // ----- World reference -----
        // SetWorld registers the persistent EDITOR world (authoring scene).
        // World() returns the ACTIVE world for rendering/gizmo (runtime when PIE, editor otherwise).
        void SetWorld(lacrima::sim::SimulationWorld* world) { m_editorWorld = world; }
        lacrima::sim::SimulationWorld* World() const
        {
            return (m_runtimeWorld && m_mode != EditorMode::Edit)
                       ? m_runtimeWorld.get()
                       : m_editorWorld;
        }
        lacrima::sim::SimulationWorld* EditorWorld() const { return m_editorWorld; }
        lacrima::sim::SimulationWorld* RuntimeWorld() const { return m_runtimeWorld.get(); }

        // ----- Camera -----
        EditorCamera& Camera() { return m_camera; }
        const EditorCamera& Camera() const { return m_camera; }

        // ----- Viewport & Scene Texture -----
        void SetSceneTexture(void* desc) { m_sceneTexture = desc; }
        void* GetSceneTexture() const { return m_sceneTexture; }

        void RequestViewportResize(u32 w, u32 h)
        {
            if (w != m_viewportWidth || h != m_viewportHeight)
            {
                m_viewportWidth = w;
                m_viewportHeight = h;
                m_viewportResizeRequested = true;
            }
        }

        bool ConsumeViewportResizeRequest(u32& outW, u32& outH)
        {
            if (m_viewportResizeRequested)
            {
                outW = m_viewportWidth;
                outH = m_viewportHeight;
                m_viewportResizeRequested = false;
                return true;
            }
            return false;
        }

        // ----- Undo/Redo -----
        void ExecuteCommand(EditorCommand cmd);
        void Undo();
        void Redo();
        bool CanUndo() const { return !m_undoStack.empty(); }
        bool CanRedo() const { return !m_redoStack.empty(); }

        // ----- Viewport fullscreen toggle -----
        bool IsViewportFullscreen() const { return m_viewportFullscreen; }
        void ToggleViewportFullscreen() { m_viewportFullscreen = !m_viewportFullscreen; }

    private:
        lacrima::Entity                          m_selectedEntity{};
        EditorMode                               m_mode = EditorMode::Edit;

        // Dual-world PIE: editor world is the persistent authoring scene;
        // runtime world is a clone that is created on PIEPlay and destroyed on PIEStop.
        lacrima::sim::SimulationWorld*           m_editorWorld  = nullptr;
        std::unique_ptr<lacrima::sim::SimulationWorld> m_runtimeWorld;

        bool                                     m_viewportFullscreen = false;

        EditorCamera                             m_camera;
        void*                                    m_sceneTexture = nullptr;
        u32                                      m_viewportWidth  = 1280;
        u32                                      m_viewportHeight = 720;
        bool                                     m_viewportResizeRequested = false;

        std::vector<EditorCommand>               m_undoStack;
        std::vector<EditorCommand>               m_redoStack;
    };
}
