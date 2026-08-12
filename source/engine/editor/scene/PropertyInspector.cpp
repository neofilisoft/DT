#include "editor/scene/PropertyInspector.h"
#include "editor/core/EditorContext.h"
#include "simulation/world/SimulationWorld.h"
#include "simulation/spatial/TransformComponent.h"
#include "simulation/needs/NeedsComponent.h"
#include "simulation/navigation/NavAgentComponent.h"
#include "simulation/interaction/InteractionQueue.h"

#include <imgui.h>
#include <string>

namespace dt::editor
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

        dt::Entity sel = ctx.SelectedEntity();
        ImGui::Text("Entity #%u  (gen %u)", sel.index, sel.generation);
        ImGui::Separator();

        DrawTransformSection(ctx);
        DrawNeedsSection(ctx);
        DrawNavAgentSection(ctx);
        DrawInteractionQueueSection(ctx);

        ImGui::End();
    }

    void PropertyInspector::DrawTransformSection(EditorContext& ctx)
    {
        dt::sim::SimulationWorld* world = ctx.World();
        if (!world) return;

        dt::sim::TransformComponent* tr = world->GetTransform(ctx.SelectedEntity());
        if (!tr) return;

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("X", &tr->x, 0.1f);
            ImGui::DragFloat("Y", &tr->y, 0.1f);
            ImGui::DragFloat("Z", &tr->z, 0.1f);
            ImGui::DragFloat("Yaw (rad)", &tr->yaw, 0.01f, -3.14159f, 3.14159f);
        }
    }

    void PropertyInspector::DrawNeedsSection(EditorContext& ctx)
    {
        dt::sim::SimulationWorld* world = ctx.World();
        if (!world) return;

        dt::sim::NeedsComponent* needs = world->GetNeeds(ctx.SelectedEntity());
        if (!needs) return;

        if (ImGui::CollapsingHeader("Needs"))
        {
            for (u32 i = 0; i < dt::sim::kNeedCount; ++i)
            {
                float v = needs->values[i];
                std::string label = "Need[" + std::to_string(i) + "]";
                ImGui::ProgressBar(v, ImVec2(-1.0f, 0.0f), label.c_str());
            }
        }
    }

    void PropertyInspector::DrawNavAgentSection(EditorContext& ctx)
    {
        dt::sim::SimulationWorld* world = ctx.World();
        if (!world) return;

        dt::sim::NavAgentComponent* nav = world->NavAgents().Get(ctx.SelectedEntity());
        if (!nav) return;

        if (ImGui::CollapsingHeader("NavAgent"))
        {
            ImGui::Text("State: %s", nav->hasPath ? "Moving" : "Idle");
            ImGui::Text("Goal: (%.2f, %.2f, %.2f)", (nav->hasPath && !nav->currentPath.empty()) ? nav->currentPath.back().x : 0.0f, (nav->hasPath && !nav->currentPath.empty()) ? nav->currentPath.back().y : 0.0f, (nav->hasPath && !nav->currentPath.empty()) ? nav->currentPath.back().z : 0.0f);
        }
    }

    void PropertyInspector::DrawInteractionQueueSection(EditorContext& ctx)
    {
        dt::sim::SimulationWorld* world = ctx.World();
        if (!world) return;

        dt::sim::InteractionQueue* queue = world->GetQueue(ctx.SelectedEntity());
        if (!queue) return;

        if (ImGui::CollapsingHeader("Interaction Queue"))
        {
            ImGui::Text("Queue size: %zu", queue->Size());
            if (queue->Size() > 0)
            {
                ImGui::Text("Front: %s", queue->Front()->def->name.c_str());
            }
        }
    }
}
