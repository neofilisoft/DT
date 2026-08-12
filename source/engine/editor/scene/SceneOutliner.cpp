#include "editor/scene/SceneOutliner.h"
#include "editor/core/EditorContext.h"
#include "simulation/world/SimulationWorld.h"

#include <imgui.h>
#include <cstring>
#include <string>

namespace dt::editor
{
    void SceneOutliner::Init(EditorContext& ctx)
    {
        (void)ctx;
        std::memset(m_searchBuf, 0, sizeof(m_searchBuf));
    }

    void SceneOutliner::Construct(EditorContext& ctx)
    {
        ImGui::Begin("Scene Outliner", &m_isOpen);

        // Search filter
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##search", "Search entities...", m_searchBuf, sizeof(m_searchBuf));
        ImGui::Separator();

        dt::sim::SimulationWorld* world = ctx.World();
        if (!world)
        {
            ImGui::TextDisabled("No world loaded.");
            ImGui::End();
            return;
        }

        // Entity count badge
        usize liveCount = world->LiveEntityCount();
        ImGui::TextDisabled("Entities: %zu", liveCount);
        ImGui::Separator();

        // Iterate all component arrays to enumerate live entities.
        // We use Transforms as the canonical source of entity identity - every
        // entity has a transform in this engine.
        std::string searchStr = m_searchBuf;
        world->Transforms().ForEach([&](dt::Entity entity, auto& /*transform*/)
        {
            std::string label = "Entity #" + std::to_string(entity.index);

            // Apply search filter
            if (!searchStr.empty() && label.find(searchStr) == std::string::npos)
                return;

            DrawEntityRow(ctx, entity, label);
        });

        ImGui::End();
    }

    void SceneOutliner::DrawEntityRow(EditorContext& ctx, dt::Entity entity, const std::string& label)
    {
        bool isSelected = (ctx.SelectedEntity() == entity);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf
                                 | ImGuiTreeNodeFlags_SpanAvailWidth
                                 | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (isSelected)
            flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::TreeNodeEx(label.c_str(), flags);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            ctx.Select(entity);
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Deselect"))
                ctx.ClearSelection();
            ImGui::EndPopup();
        }
    }
}
