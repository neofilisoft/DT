#include "editor/core/EditorContext.h"

namespace dt::editor
{
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
