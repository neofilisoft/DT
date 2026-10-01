// Copyright Neofilisoft. All Rights Reserved.
#include "editor/core/EditorContext.h"
#include "simulation/world/SimulationWorld.h"
#include "core/logging/Logger.h"

namespace lacrima::editor
{
    EditorContext::EditorContext() = default;
    EditorContext::~EditorContext() = default;

    // ----- PIE Lifecycle -------------------------------------------------
    // PIEPlay: snapshot the editor world into a runtime clone and start sim.
    // The editor world is NEVER ticked or mutated while PIE is running.
    void EditorContext::PIEPlay()
    {
        if (!m_editorWorld)
        {
            LACRIMA_LOG_WARN(lacrima::LogCategory::Core,
                             "PIEPlay: no editor world set - cannot enter Play mode.");
            return;
        }

        if (m_mode != EditorMode::Edit)
        {
            // Already in PIE - resume from Paused/Stepping to Play.
            m_mode = EditorMode::Play;
            return;
        }

        // Clone the editor world. Clone() performs a deep copy of every
        // ComponentArray and entity allocator so the runtime world is
        // fully independent of the authoring scene.
        m_runtimeWorld = m_editorWorld->Clone();
        if (!m_runtimeWorld)
        {
            LACRIMA_LOG_ERROR(lacrima::LogCategory::Core,
                              "PIEPlay: SimulationWorld::Clone() returned nullptr.");
            return;
        }

        m_mode = EditorMode::Play;
        LACRIMA_LOG_INFO(lacrima::LogCategory::Core,
                         "PIE: Entered Play mode. Runtime world cloned from editor world.");
    }

    // PIEPause: freeze simulation delta-time; rendering + camera orbit continue.
    void EditorContext::PIEPause()
    {
        if (m_mode == EditorMode::Play)
        {
            m_mode = EditorMode::Paused;
            LACRIMA_LOG_INFO(lacrima::LogCategory::Core, "PIE: Paused.");
        }
        else if (m_mode == EditorMode::Paused || m_mode == EditorMode::Stepping)
        {
            m_mode = EditorMode::Play;
            LACRIMA_LOG_INFO(lacrima::LogCategory::Core, "PIE: Resumed.");
        }
    }

    // PIEStep: advance the simulation by exactly 1 fixed tick (1/60 s) then pause.
    // The application tick loop checks ShouldTickRuntime() and calls
    // ConsumePIEStepRequest() after the tick to return to Paused.
    void EditorContext::PIEStep()
    {
        if (m_mode == EditorMode::Paused || m_mode == EditorMode::Play)
        {
            m_mode = EditorMode::Stepping;
            LACRIMA_LOG_INFO(lacrima::LogCategory::Core, "PIE: Stepping 1 frame.");
        }
        else if (m_mode == EditorMode::Edit)
        {
            // Allow stepping even from Edit mode: auto-enter PIE for 1 frame.
            PIEPlay();
            if (m_mode == EditorMode::Play)
                m_mode = EditorMode::Stepping;
        }
    }

    // PIEStop: teardown runtime world and restore the unmodified editor world.
    void EditorContext::PIEStop()
    {
        if (m_mode == EditorMode::Edit) return;

        m_runtimeWorld.reset(); // destroys the clone; editor world untouched
        m_mode = EditorMode::Edit;
        m_selectedEntity = lacrima::Entity{};  // clear selection: runtime entities are gone

        LACRIMA_LOG_INFO(lacrima::LogCategory::Core,
                         "PIE: Stopped. Runtime world destroyed. Editor world restored.");
    }

    // ----- Undo/Redo -----------------------------------------------------
    void EditorContext::ExecuteCommand(EditorCommand cmd)
    {
        cmd.execute();
        m_undoStack.push_back(std::move(cmd));
        // Committing a new action clears the redo history.
        m_redoStack.clear();
    }

    void EditorContext::Undo()
    {
        if (m_undoStack.empty()) return;
        EditorCommand& cmd = m_undoStack.back();
        cmd.undo();
        m_redoStack.push_back(std::move(cmd));
        m_undoStack.pop_back();
    }

    void EditorContext::Redo()
    {
        if (m_redoStack.empty()) return;
        EditorCommand& cmd = m_redoStack.back();
        cmd.execute();
        m_undoStack.push_back(std::move(cmd));
        m_redoStack.pop_back();
    }
}
