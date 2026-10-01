// Copyright Neofilisoft. All Rights Reserved.
#include "editor/scene/ToolbarPanel.h"
#include "editor/core/EditorContext.h"
#include <imgui.h>

namespace lacrima::editor
{
    void ToolbarPanel::Init(EditorContext& ctx)
    {
        (void)ctx;
    }

    void ToolbarPanel::Construct(EditorContext& ctx)
    {
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                               | ImGuiWindowFlags_NoScrollWithMouse
                               | ImGuiWindowFlags_NoTitleBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0.0f, 0.0f));

        if (ImGui::Begin(Title().c_str(), &m_isOpen, flags))
        {
            float size = ImGui::GetWindowHeight() - 4.0f;

            EditorMode mode      = ctx.Mode();
            bool       isEditing = (mode == EditorMode::Edit);
            bool       isPlaying = (mode == EditorMode::Play);
            bool       isPaused  = (mode == EditorMode::Paused || mode == EditorMode::Stepping);

            // --- Centering: Play + Pause + Step + Stop = 4 buttons ---
            float totalWidth = (size * 4.0f) + (ImGui::GetStyle().ItemSpacing.x * 3.0f);
            ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (totalWidth * 0.5f));

            // ---- Play button (green when active) ----
            ImVec4 playColor = isPlaying
                ? ImVec4(0.20f, 0.85f, 0.30f, 1.0f)   // bright green = playing
                : ImVec4(0.55f, 0.85f, 0.55f, 1.0f);  // dim green = idle

            ImGui::PushStyleColor(ImGuiCol_Text, playColor);
            if (ImGui::Button("##PIEPlay", ImVec2(size, size)))
            {
                // From Edit -> clone world and Play.
                // From Paused/Stepping -> resume Play.
                ctx.PIEPlay();
            }
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(isEditing ? "Play  (clones editor world)" : "Resume");

            // Draw icon text centered inside button manually via window draw list
            {
                ImVec2 bMin = ImGui::GetItemRectMin();
                ImVec2 bMax = ImGui::GetItemRectMax();
                ImVec2 center = ImVec2((bMin.x + bMax.x) * 0.5f - 4.0f, (bMin.y + bMax.y) * 0.5f - 7.0f);
                ImGui::GetWindowDrawList()->AddText(center, isPlaying
                    ? IM_COL32(60, 220, 80, 255)
                    : IM_COL32(140, 200, 140, 255), ">");
            }

            ImGui::SameLine();

            // ---- Pause button (yellow when paused) ----
            ImVec4 pauseColor = isPaused
                ? ImVec4(1.0f, 0.85f, 0.20f, 1.0f)
                : ImGui::GetStyle().Colors[ImGuiCol_Text];

            ImGui::PushStyleColor(ImGuiCol_Text, pauseColor);
            if (ImGui::Button("##PIEPause", ImVec2(size, size)))
            {
                ctx.PIEPause();
            }
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(isPaused ? "Resume" : "Pause");

            {
                ImVec2 bMin = ImGui::GetItemRectMin();
                ImVec2 bMax = ImGui::GetItemRectMax();
                ImVec2 center = ImVec2((bMin.x + bMax.x) * 0.5f - 4.0f, (bMin.y + bMax.y) * 0.5f - 7.0f);
                ImGui::GetWindowDrawList()->AddText(center, isPaused
                    ? IM_COL32(255, 220, 50, 255)
                    : IM_COL32(180, 180, 180, 255), "||");
            }

            ImGui::SameLine();

            // ---- Step button (single-frame advance; only active when paused) ----
            bool canStep = isPaused || isEditing;
            if (!canStep) ImGui::BeginDisabled();

            if (ImGui::Button("##PIEStep", ImVec2(size, size)))
            {
                ctx.PIEStep();
            }

            if (!canStep) ImGui::EndDisabled();

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Step 1 Frame (1/60 s)");

            {
                ImVec2 bMin = ImGui::GetItemRectMin();
                ImVec2 bMax = ImGui::GetItemRectMax();
                ImVec2 center = ImVec2((bMin.x + bMax.x) * 0.5f - 5.0f, (bMin.y + bMax.y) * 0.5f - 7.0f);
                ImGui::GetWindowDrawList()->AddText(center, canStep
                    ? IM_COL32(200, 220, 255, 255)
                    : IM_COL32(100, 100, 100, 255), "|>");
            }

            ImGui::SameLine();

            // ---- Stop button (red; disabled in Edit mode) ----
            if (isEditing) ImGui::BeginDisabled();

            ImVec4 stopColor = !isEditing
                ? ImVec4(0.90f, 0.25f, 0.25f, 1.0f)
                : ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];

            ImGui::PushStyleColor(ImGuiCol_Text, stopColor);
            if (ImGui::Button("##PIEStop", ImVec2(size, size)))
            {
                ctx.PIEStop();
            }
            ImGui::PopStyleColor();

            if (isEditing) ImGui::EndDisabled();

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Stop  (destroys runtime world, restores editor scene)");

            {
                ImVec2 bMin = ImGui::GetItemRectMin();
                ImVec2 bMax = ImGui::GetItemRectMax();
                ImVec2 center = ImVec2((bMin.x + bMax.x) * 0.5f - 4.0f, (bMin.y + bMax.y) * 0.5f - 7.0f);
                ImGui::GetWindowDrawList()->AddText(center, !isEditing
                    ? IM_COL32(230, 60, 60, 255)
                    : IM_COL32(100, 100, 100, 255), "[]");
            }

            // --- PIE mode indicator label (right side) ---
            const char* modeLabel = isEditing  ? "Edit"
                                  : isPlaying  ? "Playing..."
                                  : isPaused   ? "Paused"
                                               : "";
            ImGui::SameLine(0, 20);
            ImGui::TextDisabled("%s", modeLabel);
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
    }
}
