#pragma once
// ---------------------------------------------------------------------------
// editor/scene/PropertyInspector.h
//
// Inspector panel - shows components (Transform, Needs, NavAgent, etc.) of
// the entity selected in SceneOutliner.
// Uses DT Reflection (REFLECT_BEGIN/REFLECT_FIELD macros) where available,
// but also provides hand-written UIs for known component types because our
// reflection layer is a lightweight macro system, not a full RTTR library.
// ---------------------------------------------------------------------------

#include "editor/core/EditorPanel.h"

namespace dt::editor
{
    class PropertyInspector final : public EditorPanel
    {
    public:
        PropertyInspector() : EditorPanel("Inspector") {}

        void Construct(EditorContext& ctx) override;

    private:
        void DrawTransformSection(EditorContext& ctx);
        void DrawNeedsSection(EditorContext& ctx);
        void DrawNavAgentSection(EditorContext& ctx);
        void DrawInteractionQueueSection(EditorContext& ctx);
    };
}
