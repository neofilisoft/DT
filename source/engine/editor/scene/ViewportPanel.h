#pragma once

#include "editor/core/EditorPanel.h"
#include <ImGuizmo.h>

namespace lacrima::editor
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
        void DrawGizmo(EditorContext& ctx, const float* viewMatrix, const float* projMatrix);

        GizmoMode           m_gizmoMode   = GizmoMode::Translate;
        bool                m_localSpace  = true;
    };
}
