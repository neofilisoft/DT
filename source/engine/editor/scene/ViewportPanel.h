#pragma once
// ---------------------------------------------------------------------------
// editor/scene/ViewportPanel.h
//
// The main 3D editing viewport. Renders the engine scene into an ImGui image
// and provides ImGuizmo gizmos for translating/rotating/scaling the selected
// entity. Inspired by Lacrima SimulationUI::constructImGuizmo().
// ---------------------------------------------------------------------------

#include "editor/core/EditorPanel.h"
#include <ImGuizmo.h>

namespace dt::editor
{
    enum class GizmoMode
    {
        Select,
        Translate,
        Rotate,
        Scale,
    };

    class ViewportPanel final : public EditorPanel
    {
    public:
        ViewportPanel() : EditorPanel("Viewport") {}

        void Init(EditorContext& ctx) override;
        void Construct(EditorContext& ctx) override;

    private:
        void DrawToolbar(EditorContext& ctx);
        void DrawGizmo(EditorContext& ctx);

        GizmoMode           m_gizmoMode   = GizmoMode::Translate;
        bool                m_localSpace  = true;
        float               m_viewMatrix[16]  = {};
        float               m_projMatrix[16]  = {};
    };
}
