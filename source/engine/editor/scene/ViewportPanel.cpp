// Copyright Neofilisoft. All Rights Reserved.
#include "editor/scene/ViewportPanel.h"
#include "editor/core/EditorContext.h"
#include "simulation/world/SimulationWorld.h"
#include "simulation/spatial/SpringArmComponent.h"
#include "simulation/spatial/TransformComponent.h"
#include "core/logging/Logger.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <cstring>
#include <cmath>

namespace lacrima::editor
{
    // -------------------------------------------------------------------------
    // Helper: build a column-major 4x4 identity matrix in-place.
    // -------------------------------------------------------------------------
    static void MatIdentity(float* m)
    {
        std::memset(m, 0, sizeof(float) * 16);
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    // -------------------------------------------------------------------------
    // Helper: compose a 4x4 TRS matrix from TransformComponent (pos + yaw only;
    // TransformComponent does not yet carry pitch/roll/scale so those remain
    // identity). ImGuizmo decomposes this matrix back via its own decompose.
    // -------------------------------------------------------------------------
    static void MatFromTransform(const lacrima::sim::TransformComponent& tr, float* m)
    {
        MatIdentity(m);
        m[12] = tr.x;
        m[13] = tr.y;
        m[14] = tr.z;

        float cosY = std::cos(tr.yaw);
        float sinY = std::sin(tr.yaw);
        m[0]  =  cosY;
        m[2]  =  sinY;
        m[8]  = -sinY;
        m[10] =  cosY;
    }

    // -------------------------------------------------------------------------
    // Helper: decompose a column-major 4x4 matrix back into TransformComponent.
    // Extracts translation and yaw (Y-axis rotation) only.
    // -------------------------------------------------------------------------
    static void ApplyMatToTransform(const float* m, lacrima::sim::TransformComponent& tr)
    {
        tr.x   = m[12];
        tr.y   = m[13];
        tr.z   = m[14];
        tr.yaw = std::atan2(m[2], m[0]);
    }

    // =========================================================================
    void ViewportPanel::Init(EditorContext& ctx)
    {
        (void)ctx;
    }

    // =========================================================================
    void ViewportPanel::Construct(EditorContext& ctx)
    {
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar
                                     | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        bool windowOpen = ImGui::Begin(Title().c_str(), &m_isOpen, windowFlags);
        ImGui::PopStyleVar();

        if (!windowOpen)
        {
            ImGui::End();
            return;
        }

        ImVec2 viewportPos  = ImGui::GetCursorScreenPos();
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        if (viewportSize.x < 16.0f) viewportSize.x = 16.0f;
        if (viewportSize.y < 16.0f) viewportSize.y = 16.0f;

        // Sync viewport resolution with camera and offscreen target.
        ctx.Camera().SetViewportSize(viewportSize.x, viewportSize.y);
        ctx.RequestViewportResize(static_cast<u32>(viewportSize.x), static_cast<u32>(viewportSize.y));

        // ---- Camera and PIE player input ----
        bool isHovered = ImGui::IsWindowHovered();
        ImGuiIO& io    = ImGui::GetIO();

        bool hasActiveSpringArm = false;
        if (ctx.IsInPIE() && ctx.World())
        {
            lacrima::sim::SimulationWorld* world = ctx.World();
            world->SpringArms().ForEach([&](lacrima::Entity ent, lacrima::sim::SpringArmComponent& sa)
            {
                hasActiveSpringArm = true;
                if (isHovered)
                {
                    // RMB orbits the 3rd person camera
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
                    {
                        sa.targetYaw   += io.MouseDelta.x * 0.005f;
                        sa.targetPitch += io.MouseDelta.y * 0.005f;
                        sa.targetPitch = std::clamp(sa.targetPitch, -1.4f, 1.4f);
                    }

                    // Mouse wheel adjusts zoom distance
                    if (std::abs(io.MouseWheel) > 0.001f)
                    {
                        sa.targetArmLength = std::clamp(sa.targetArmLength - io.MouseWheel * 0.5f, sa.minArmLength, sa.maxArmLength);
                    }

                    // WASD movement relative to camera yaw
                    if (!io.WantCaptureKeyboard && world->Transforms().Has(ent))
                    {
                        float moveSpeed = 5.0f * (1.0f / 60.0f);
                        float forwardX = std::sin(sa.currentYaw);
                        float forwardZ = std::cos(sa.currentYaw);
                        float rightX   = forwardZ;
                        float rightZ   = -forwardX;

                        auto* tr = world->Transforms().Get(ent);
                        if (tr)
                        {
                            if (ImGui::IsKeyDown(ImGuiKey_W)) { tr->x += forwardX * moveSpeed; tr->z += forwardZ * moveSpeed; }
                            if (ImGui::IsKeyDown(ImGuiKey_S)) { tr->x -= forwardX * moveSpeed; tr->z -= forwardZ * moveSpeed; }
                            if (ImGui::IsKeyDown(ImGuiKey_D)) { tr->x += rightX   * moveSpeed; tr->z += rightZ   * moveSpeed; }
                            if (ImGui::IsKeyDown(ImGuiKey_A)) { tr->x -= rightX   * moveSpeed; tr->z -= rightZ   * moveSpeed; }
                        }
                    }
                }

                // Sync Viewport EditorCamera with SpringArm
                ctx.Camera().SetFocalPoint(sa.computedLookAtTarget);
                ctx.Camera().SetDistance(sa.currentArmLength);
                ctx.Camera().SetRotation(sa.currentPitch, sa.currentYaw);
            });
        }

        if (!hasActiveSpringArm && isHovered)
        {
            bool rmb   = ImGui::IsMouseDown(ImGuiMouseButton_Right);
            bool mmb   = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
            bool shift = io.KeyShift;

            if (rmb || mmb)
            {
                ctx.Camera().OnMouseMove(io.MouseDelta.x, io.MouseDelta.y,
                                         rmb && !shift, mmb || (rmb && shift));
            }

            if (std::abs(io.MouseWheel) > 0.001f)
                ctx.Camera().OnMouseScroll(io.MouseWheel);

            // ---- Keyboard shortcuts for Gizmo mode (Q/W/E/R) ----
            // Only when viewport is hovered and no text input is captured.
            if (!io.WantCaptureKeyboard)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_Q)) m_gizmoMode = GizmoMode::Select;
                if (ImGui::IsKeyPressed(ImGuiKey_W)) m_gizmoMode = GizmoMode::Translate;
                if (ImGui::IsKeyPressed(ImGuiKey_E)) m_gizmoMode = GizmoMode::Rotate;
                if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmoMode = GizmoMode::Scale;
            }
        }
        ctx.Camera().Update();

        // ---- Render scene texture (fills 100% of the viewport area) ----
        void* sceneTex = ctx.GetSceneTexture();
        if (sceneTex != nullptr)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(sceneTex), viewportSize);
        }
        else
        {
            ImGui::GetWindowDrawList()->AddRectFilled(
                viewportPos,
                ImVec2(viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y),
                IM_COL32(24, 24, 28, 255));

            ImGui::GetWindowDrawList()->AddText(
                ImVec2(viewportPos.x + 12.0f, viewportPos.y + 12.0f),
                IM_COL32(160, 160, 180, 255),
                "Initializing 3D Viewport...");
        }

        // ---- Floating overlay toolbar (Gizmo mode + coordinate space toggle) ----
        ImGui::SetCursorScreenPos(ImVec2(viewportPos.x + 10.0f, viewportPos.y + 10.0f));
        DrawToolbar(ctx);

        // ---- Drag-and-drop asset spawn ----
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ASSET_GLB"))
            {
                const char* droppedPath = static_cast<const char*>(payload->Data);
                LACRIMA_LOG_INFO(lacrima::LogCategory::Core,
                                 "Viewport: Dropped asset: %s", droppedPath);

                if (lacrima::sim::SimulationWorld* world = ctx.World())
                {
                    lacrima::Entity newEntity = world->CreateEntity();

                    lacrima::sim::TransformComponent tr{};
                    Vec3 focal = ctx.Camera().GetFocalPoint();
                    tr.x = focal.x;
                    tr.y = focal.y;
                    tr.z = focal.z;
                    world->Transforms().Add(newEntity, tr);

                    lacrima::sim::VisualComponent vis{};
                    vis.assetPath  = lacrima::StringID(droppedPath);
                    vis.color[0]   = 1.0f;
                    vis.color[1]   = 1.0f;
                    vis.color[2]   = 1.0f;
                    vis.color[3]   = 1.0f;
                    world->Visuals().Add(newEntity, vis);

                    ctx.Select(newEntity);
                    LACRIMA_LOG_INFO(lacrima::LogCategory::Core,
                                     "Viewport: Spawned Entity with asset %s", droppedPath);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // ---- ImGuizmo setup (must happen after scene Image to share draw list) ----
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

        const float* viewMtx = ctx.Camera().GetViewMatrix().m;
        const float* projMtx = ctx.Camera().GetProjectionMatrix().m;

        // Reference grid (20 m x 20 m around origin)
        float gridIdentity[16];
        MatIdentity(gridIdentity);
        ImGuizmo::DrawGrid(viewMtx, projMtx, gridIdentity, 20.0f);

        DrawGizmo(ctx, viewMtx, projMtx);

        ImGui::End();
    }

    // =========================================================================
    // DrawToolbar - floating overlay showing Gizmo mode buttons and space toggle.
    // PIE controls (Play/Pause/Step/Stop) live in ToolbarPanel, not here, to
    // avoid duplication. This overlay is for per-viewport controls only.
    // =========================================================================
    void ViewportPanel::DrawToolbar(EditorContext& ctx)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.11f, 0.15f, 0.88f));

        if (ImGui::BeginChild("##VpToolbar", ImVec2(0.0f, 34.0f), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::BeginGroup();

            // -- PIE mode badge (read-only indicator, no buttons here) --
            const char* modeLabel = nullptr;
            ImU32       modeBadgeColor = IM_COL32(160, 160, 180, 255);
            switch (ctx.Mode())
            {
                case EditorMode::Edit:     modeLabel = "Edit";     modeBadgeColor = IM_COL32(140,140,160,255); break;
                case EditorMode::Play:     modeLabel = "Playing";  modeBadgeColor = IM_COL32( 60,210, 80,255); break;
                case EditorMode::Paused:   modeLabel = "Paused";   modeBadgeColor = IM_COL32(240,190, 30,255); break;
                case EditorMode::Stepping: modeLabel = "Stepping"; modeBadgeColor = IM_COL32(120,180,255,255); break;
            }
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(modeBadgeColor), "[%s]", modeLabel);
            ImGui::SameLine(0, 12);

            // -- Gizmo mode radio buttons (Q / W / E / R) --
            auto GizmoBtn = [&](const char* label, GizmoMode gm, const char* tooltip)
            {
                bool active = (m_gizmoMode == gm);
                if (active)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.26f, 0.59f, 0.98f, 1.00f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.69f, 1.00f, 1.00f));
                }
                if (ImGui::SmallButton(label)) m_gizmoMode = gm;
                if (active) ImGui::PopStyleColor(2);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
                ImGui::SameLine(0, 2);
            };

            GizmoBtn("Q Select",    GizmoMode::Select,    "Select mode  (Q)");
            GizmoBtn("W Translate", GizmoMode::Translate, "Translate    (W)");
            GizmoBtn("E Rotate",    GizmoMode::Rotate,    "Rotate       (E)");
            GizmoBtn("R Scale",     GizmoMode::Scale,     "Scale        (R)");

            ImGui::SameLine(0, 12);

            // -- Coordinate space toggle (World / Local) --
            if (ImGui::SmallButton(m_localSpace ? "Local" : "World"))
                m_localSpace = !m_localSpace;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Toggle coordinate space (Local / World)");

            // -- Snapping indicator (Ctrl = snap active) --
            ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl)
            {
                ImGui::SameLine(0, 8);
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "[Snap]");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Grid snap active  T:0.5m  R:15deg  S:0.25");
            }

            ImGui::EndGroup();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    // =========================================================================
    // DrawGizmo - renders the ImGuizmo manipulator and writes delta back to the
    // selected entity's TransformComponent via the undo/redo command system.
    //
    // Snapping: hold Ctrl to snap T=0.5m, R=15deg, S=0.25.
    // Gizmo runs on the ACTIVE world (runtime world when in PIE, editor otherwise).
    // =========================================================================
    void ViewportPanel::DrawGizmo(EditorContext& ctx, const float* viewMatrix, const float* projMatrix)
    {
        if (m_gizmoMode == GizmoMode::Select) return;
        if (!ctx.HasSelection())              return;

        lacrima::sim::SimulationWorld* world = ctx.World();
        if (!world) return;

        lacrima::sim::TransformComponent* tr = world->GetTransform(ctx.SelectedEntity());
        if (!tr) return;

        float objectMatrix[16];
        MatFromTransform(*tr, objectMatrix);

        ImGuizmo::OPERATION op = (m_gizmoMode == GizmoMode::Translate) ? ImGuizmo::TRANSLATE
                               : (m_gizmoMode == GizmoMode::Rotate)    ? ImGuizmo::ROTATE
                                                                        : ImGuizmo::SCALE;
        ImGuizmo::MODE spaceMode = m_localSpace ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        // Snap values: hold Ctrl to activate (matches plan: T=0.5m, R=15deg, S=0.25).
        ImGuiIO& io = ImGui::GetIO();
        float snapT[3] = { 0.5f, 0.5f, 0.5f };
        float snapR[3] = { 15.0f, 15.0f, 15.0f };
        float snapS[3] = { 0.25f, 0.25f, 0.25f };
        float* snap = nullptr;
        if (io.KeyCtrl)
        {
            snap = (op == ImGuizmo::TRANSLATE) ? snapT
                 : (op == ImGuizmo::ROTATE)    ? snapR
                                               : snapS;
        }

        float delta[16];
        bool manipulated = ImGuizmo::Manipulate(viewMatrix, projMatrix,
                                                op, spaceMode,
                                                objectMatrix, delta,
                                                snap);
        if (manipulated)
        {
            lacrima::sim::TransformComponent oldTr = *tr;
            ApplyMatToTransform(objectMatrix, *tr);
            lacrima::sim::TransformComponent newTr = *tr;
            lacrima::Entity entity = ctx.SelectedEntity();

            // Push to undo/redo stack only in Edit mode; PIE mutations are discarded on Stop.
            if (ctx.Mode() == EditorMode::Edit)
            {
                ctx.ExecuteCommand({
                    "Transform Entity",
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
}
