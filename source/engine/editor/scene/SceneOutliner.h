#pragma once
// ---------------------------------------------------------------------------
// editor/scene/SceneOutliner.h
//
// Displays all live entities in the SimulationWorld as a selectable list.
// Clicking an entity updates EditorContext::SelectedEntity() which the
// PropertyInspector and ViewportPanel (ImGuizmo) respond to.
// ---------------------------------------------------------------------------

#include "editor/core/EditorPanel.h"
#include "runtime/Entity.h"

namespace dt::editor
{
    class SceneOutliner final : public EditorPanel
    {
    public:
        SceneOutliner() : EditorPanel("Scene Outliner") {}

        void Init(EditorContext& ctx) override;
        void Construct(EditorContext& ctx) override;

    private:
        void DrawEntityRow(EditorContext& ctx, dt::Entity entity, const std::string& label);

        char m_searchBuf[128] = {};
    };
}
