// Copyright Neofilisoft. All Rights Reserved.
#include "editor/scene/PropertyInspector.h"
#include "editor/core/EditorContext.h"
#include "simulation/world/SimulationWorld.h"
#include "simulation/spatial/TransformComponent.h"
#include "simulation/navigation/NavAgentComponent.h"

#include <imgui.h>

namespace lacrima::editor
{
    void PropertyInspector::Construct(EditorContext& ctx)
    {
        ImGui::Begin("Inspector", &m_isOpen);

        if (!ctx.HasSelection())
        {
            ImGui::TextDisabled("No entity selected.");
            ImGui::End();
            return;
        }

        lacrima::Entity sel = ctx.SelectedEntity();
        ImGui::Text("Entity #%u  (gen %u)", sel.index, sel.generation);
        ImGui::Separator();

        DrawTransformSection(ctx);
        DrawNavAgentSection(ctx);

        ImGui::End();
    }

    void PropertyInspector::DrawTransformSection(EditorContext& ctx)
    {
        lacrima::sim::SimulationWorld* world = ctx.World();
        if (!world) return;

        lacrima::sim::TransformComponent* tr = world->GetTransform(ctx.SelectedEntity());
        if (!tr) return;

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("X", &tr->x, 0.1f);
            ImGui::DragFloat("Y", &tr->y, 0.1f);
            ImGui::DragFloat("Z", &tr->z, 0.1f);
            ImGui::DragFloat("Yaw (rad)", &tr->yaw, 0.01f, -3.14159f, 3.14159f);
        }
    }

    void PropertyInspector::DrawNavAgentSection(EditorContext& ctx)
    {
        lacrima::sim::SimulationWorld* world = ctx.World();
        if (!world) return;

        lacrima::sim::NavAgentComponent* nav = world->NavAgents().Get(ctx.SelectedEntity());
        if (!nav) return;

        if (ImGui::CollapsingHeader("NavAgent"))
        {
            ImGui::Text("State: %s", nav->hasPath ? "Moving" : "Idle");
            ImGui::Text("Goal: (%.2f, %.2f, %.2f)", (nav->hasPath && !nav->currentPath.empty()) ? nav->currentPath.back().x : 0.0f, (nav->hasPath && !nav->currentPath.empty()) ? nav->currentPath.back().y : 0.0f, (nav->hasPath && !nav->currentPath.empty()) ? nav->currentPath.back().z : 0.0f);
        }
    }
}
