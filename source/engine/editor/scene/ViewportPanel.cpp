#include "editor/scene/ViewportPanel.h"
#include "editor/core/EditorContext.h"
#include "simulation/world/SimulationWorld.h"
#include "simulation/spatial/TransformComponent.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <cstring>
#include <cmath>

namespace dt::editor
{
    // Minimal helper: build an identity 4x4 float matrix
    static void MatIdentity(float* m)
    {
        std::memset(m, 0, sizeof(float) * 16);
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    // Build a simple translation matrix from TransformComponent
    static void MatFromTransform(const dt::sim::TransformComponent& tr, float* m)
    {
        MatIdentity(m);
        m[12] = tr.x;
        m[13] = tr.y;
        m[14] = tr.z;
        // Apply yaw rotation around Y
        float cosY = std::cos(tr.yaw);
        float sinY = std::sin(tr.yaw);
        m[0]  =  cosY;
        m[2]  =  sinY;
        m[8]  = -sinY;
        m[10] =  cosY;
    }

    // Extract translation back into TransformComponent
    static void ApplyMatToTransform(const float* m, dt::sim::TransformComponent& tr)
    {
        tr.x = m[12];
        tr.y = m[13];
        tr.z = m[14];
        tr.yaw = std::atan2(m[2], m[0]);
    }

    void ViewportPanel::Init(EditorContext& ctx)
    {
        (void)ctx;
        // Build a default view/proj for the gizmo until a real camera is wired in.
        MatIdentity(m_viewMatrix);
        MatIdentity(m_projMatrix);
        // Simple perspective-like projection
        m_projMatrix[0]  = 1.0f;
        m_projMatrix[5]  = 1.0f;
        m_projMatrix[10] = -1.0f;
        m_projMatrix[11] = -1.0f;
        m_projMatrix[14] = -0.2f;
    }

    void ViewportPanel::Construct(EditorContext& ctx)
    {
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar
                                     | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport", &m_isOpen, windowFlags);
        ImGui::PopStyleVar();

        DrawToolbar(ctx);

        // Viewport rect for ImGuizmo
        ImVec2 viewportPos  = ImGui::GetCursorScreenPos();
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        // Placeholder: show a colored rectangle where the rendered scene would appear.
        // When VulkanRenderer integrates an offscreen framebuffer, this becomes
        // ImGui::Image(sceneDescriptorSet, viewportSize).
        ImGui::GetWindowDrawList()->AddRectFilled(
            viewportPos,
            ImVec2(viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y),
            IM_COL32(30, 30, 36, 255));

        ImGui::GetWindowDrawList()->AddText(
            ImVec2(viewportPos.x + 12, viewportPos.y + 12),
            IM_COL32(120, 120, 140, 255),
            "[Scene renders here - wire VulkanRenderer offscreen FB]");

        // ImGuizmo requires the viewport rect to be set each frame.
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

        DrawGizmo(ctx);

        ImGui::End();
    }

    void ViewportPanel::DrawToolbar(EditorContext& ctx)
    {
        // Mode: Edit / Play / Pause
        ImGui::BeginGroup();

        const char* modeLabel = (ctx.Mode() == EditorMode::Edit)   ? "Edit"
                              : (ctx.Mode() == EditorMode::Play)   ? "Play"
                                                                    : "Paused";
        ImGui::Text("Mode: %s", modeLabel);
        ImGui::SameLine(0, 20);

        if (ctx.Mode() == EditorMode::Edit)
        {
            if (ImGui::Button("[>] Play")) ctx.SetMode(EditorMode::Play);
        }
        else
        {
            if (ImGui::Button("[||] Pause")) ctx.SetMode(EditorMode::Paused);
            ImGui::SameLine();
            if (ImGui::Button("[sq] Stop"))  ctx.SetMode(EditorMode::Edit);
        }

        ImGui::SameLine(0, 20);

        // Gizmo mode selector
        if (ImGui::RadioButton("Select",    m_gizmoMode == GizmoMode::Select))    m_gizmoMode = GizmoMode::Select;
        ImGui::SameLine();
        if (ImGui::RadioButton("Translate", m_gizmoMode == GizmoMode::Translate)) m_gizmoMode = GizmoMode::Translate;
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate",    m_gizmoMode == GizmoMode::Rotate))    m_gizmoMode = GizmoMode::Rotate;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale",     m_gizmoMode == GizmoMode::Scale))     m_gizmoMode = GizmoMode::Scale;

        ImGui::SameLine(0, 20);
        ImGui::Checkbox("Local", &m_localSpace);

        ImGui::EndGroup();
        ImGui::Separator();
    }

    void ViewportPanel::DrawGizmo(EditorContext& ctx)
    {
        if (m_gizmoMode == GizmoMode::Select) return;
        if (!ctx.HasSelection()) return;

        dt::sim::SimulationWorld* world = ctx.World();
        if (!world) return;

        dt::sim::TransformComponent* tr = world->GetTransform(ctx.SelectedEntity());
        if (!tr) return;

        float objectMatrix[16];
        MatFromTransform(*tr, objectMatrix);

        ImGuizmo::OPERATION op = (m_gizmoMode == GizmoMode::Translate) ? ImGuizmo::TRANSLATE
                               : (m_gizmoMode == GizmoMode::Rotate)    ? ImGuizmo::ROTATE
                                                                        : ImGuizmo::SCALE;
        ImGuizmo::MODE mode    = m_localSpace ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        float delta[16];
        bool manipulated = ImGuizmo::Manipulate(m_viewMatrix, m_projMatrix,
                                                op, mode, objectMatrix, delta);
        if (manipulated)
        {
            // Record undo state before applying
            dt::sim::TransformComponent oldTr = *tr;
            ApplyMatToTransform(objectMatrix, *tr);
            dt::sim::TransformComponent newTr = *tr;
            dt::Entity entity = ctx.SelectedEntity();

            ctx.ExecuteCommand({
                "Move Entity",
                [world, entity, newTr]() mutable
                {
                    if (auto* t = world->GetTransform(entity)) *t = newTr;
                },
                [world, entity, oldTr]() mutable
                {
                    if (auto* t = world->GetTransform(entity)) *t = oldTr;
                }
            });
        }
    }
}
