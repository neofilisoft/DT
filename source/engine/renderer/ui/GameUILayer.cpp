#include "renderer/ui/GameUILayer.h"

#include "core/logging/Logger.h"

#include <imgui.h>

#include <cmath>
#include <cstdio>

namespace dt::renderer
{
    // -------------------------------------------------------------------------
    // Game UI color palette - premium dark theme, separate from ImGui defaults
    // -------------------------------------------------------------------------

    namespace GameStyle
    {
        // Background panels: very dark translucent
        static const ImVec4 kPanelBg       = { 0.04f, 0.04f, 0.08f, 0.82f };
        // Accent color: deep indigo/violet for bars and highlights
        static const ImVec4 kAccent        = { 0.38f, 0.22f, 0.72f, 1.00f };
        static const ImVec4 kAccentHover   = { 0.50f, 0.32f, 0.88f, 1.00f };
        // Need bar colors
        static const ImVec4 kNeedHigh      = { 0.20f, 0.75f, 0.40f, 1.00f }; // green
        static const ImVec4 kNeedMid       = { 0.90f, 0.72f, 0.10f, 1.00f }; // amber
        static const ImVec4 kNeedLow       = { 0.85f, 0.18f, 0.18f, 1.00f }; // red
        static const ImVec4 kText          = { 0.92f, 0.92f, 0.95f, 1.00f };
        static const ImVec4 kTextMuted     = { 0.55f, 0.55f, 0.65f, 1.00f };
        static const ImVec4 kBorder        = { 0.28f, 0.20f, 0.48f, 0.70f };
    }

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    void GameUILayer::Initialize()
    {
        // GameUILayer shares the ImGui context created by ImGuiLayer.
        // We only validate the context is live, not create a new one.
        if (!ImGui::GetCurrentContext())
        {
            DT_LOG_ERROR(LogCategory::Renderer,
                "GameUILayer: ImGui context not yet initialized. "
                "Call ImGuiLayer::Initialize() first.");
            return;
        }
        m_initialized = true;
        DT_LOG_INFO(LogCategory::Renderer, "GameUILayer: initialized");
    }

    void GameUILayer::Shutdown()
    {
        m_initialized = false;
    }

    // -------------------------------------------------------------------------
    // Style push/pop helpers
    // -------------------------------------------------------------------------

    void GameUILayer::PushGameStyle()
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg,        GameStyle::kPanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border,          GameStyle::kBorder);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,   GameStyle::kAccent);
        ImGui::PushStyleColor(ImGuiCol_Text,            GameStyle::kText);
        ImGui::PushStyleColor(ImGuiCol_FrameBg,         { 0.10f, 0.08f, 0.18f, 0.90f });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,     6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,  1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   { 10.0f, 8.0f });
    }

    void GameUILayer::PopGameStyle()
    {
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(5);
    }

    // -------------------------------------------------------------------------
    // DrawGameHUD - entry point called once per rendered frame
    // -------------------------------------------------------------------------

    void GameUILayer::DrawGameHUD(const SimSnapshot& snapshot, u32 screenW, u32 screenH)
    {
        if (!m_initialized) return;

        PushGameStyle();

        DrawDayTimeBar(snapshot, screenW);
        DrawEntityStatusPanel(snapshot, screenH);
        DrawSelectedEntityNeeds(snapshot, screenW, screenH);

        PopGameStyle();
    }

    // -------------------------------------------------------------------------
    // Sub-panels
    // -------------------------------------------------------------------------

    void GameUILayer::DrawDayTimeBar(const SimSnapshot& snapshot, u32 screenW)
    {
        // Top-center: day/time display + time-scale indicator
        const float panelW  = 280.0f;
        const float panelX  = (static_cast<float>(screenW) - panelW) * 0.5f;
        const float panelY  = 8.0f;

        ImGui::SetNextWindowPos({ panelX, panelY }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ panelW, 0.0f }, ImGuiCond_Always); // auto height
        ImGui::SetNextWindowBgAlpha(0.82f);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoInputs     |
            ImGuiWindowFlags_NoNav        |
            ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("##GameHUD_DayTime", nullptr, flags))
        {
            // Compute day / hour / minute from simTimeSeconds
            const double totalSec = snapshot.simTimeSeconds;
            const int totalMin    = static_cast<int>(totalSec / 60.0);
            const int simDay      = (totalMin / (24 * 60)) + 1;
            const int simHour     = (totalMin / 60) % 24;
            const int simMin      = totalMin % 60;

            ImGui::TextColored(GameStyle::kAccent, "Day %d  -  %02d:%02d", simDay, simHour, simMin);

            if (snapshot.timeScale == 0.0f)
            {
                ImGui::SameLine();
                ImGui::TextColored({ 0.95f, 0.35f, 0.20f, 1.0f }, "  [PAUSED]");
            }
            else if (snapshot.timeScale > 1.0f)
            {
                ImGui::SameLine();
                ImGui::TextColored(GameStyle::kTextMuted, "  x%.0f", snapshot.timeScale);
            }
        }
        ImGui::End();
    }

    void GameUILayer::DrawEntityStatusPanel(const SimSnapshot& snapshot, u32 screenH)
    {
        // Left side: list of entities with a mini status icon
        if (snapshot.proxies.empty()) return;

        const float panelW = 160.0f;
        const float panelH = std::min(static_cast<float>(screenH) * 0.45f, 300.0f);
        const float panelX = 8.0f;
        const float panelY = static_cast<float>(screenH) * 0.20f;

        ImGui::SetNextWindowPos({ panelX, panelY }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ panelW, panelH }, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.78f);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("##GameHUD_Entities", nullptr, flags))
        {
            ImGui::TextColored(GameStyle::kTextMuted, "HOUSEHOLD");
            ImGui::Separator();

            for (int i = 0; i < static_cast<int>(snapshot.proxies.size()); ++i)
            {
                const bool selected = (i == m_selectedIdx);
                char label[32];
                std::snprintf(label, sizeof(label), "Sim #%d", i + 1);

                ImGui::PushStyleColor(ImGuiCol_Button,
                    selected ? GameStyle::kAccent : ImVec4{ 0.15f, 0.10f, 0.25f, 0.80f });
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GameStyle::kAccentHover);

                if (ImGui::Button(label, { panelW - 20.0f, 26.0f }))
                    m_selectedIdx = i;

                ImGui::PopStyleColor(2);
            }
        }
        ImGui::End();
    }

    void GameUILayer::DrawSelectedEntityNeeds(const SimSnapshot& snapshot, u32 screenW, u32 screenH)
    {
        // Bottom-center: needs bars for the selected entity
        // Note: Needs data is not in the snapshot yet (snapshot has positionX/Y/Z,
        // visualId etc). Once NeedsProxy is added to RenderProxy this panel will
        // show real need values. For now it shows placeholder bars.
        (void)snapshot;

        const float panelW = 360.0f;
        const float panelH = 110.0f;
        const float panelX = (static_cast<float>(screenW) - panelW) * 0.5f;
        const float panelY = static_cast<float>(screenH) - panelH - 12.0f;

        ImGui::SetNextWindowPos({ panelX, panelY }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ panelW, panelH }, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.82f);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoInputs     |
            ImGuiWindowFlags_NoNav        |
            ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("##GameHUD_Needs", nullptr, flags))
        {
            if (!snapshot.proxies.empty() &&
                m_selectedIdx < static_cast<int>(snapshot.proxies.size()))
            {
                ImGui::TextColored(GameStyle::kTextMuted,
                    "Sim #%d - Needs", m_selectedIdx + 1);

                // Placeholder bars (real values come once NeedsProxy lands in snapshot)
                struct PlaceholderNeed { const char* name; float value; };
                const PlaceholderNeed needs[] = {
                    { "Hunger",    0.65f },
                    { "Energy",    0.80f },
                    { "Social",    0.40f },
                    { "Hygiene",   0.90f },
                };

                for (const auto& need : needs)
                {
                    ImVec4 color = (need.value > 0.6f) ? GameStyle::kNeedHigh
                                 : (need.value > 0.3f) ? GameStyle::kNeedMid
                                 :                        GameStyle::kNeedLow;

                    ImGui::TextColored(GameStyle::kTextMuted, "%-9s", need.name);
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
                    char barId[32];
                    std::snprintf(barId, sizeof(barId), "##%s", need.name);
                    ImGui::ProgressBar(need.value, { -1.0f, 12.0f }, "");
                    ImGui::PopStyleColor();
                }
            }
            else
            {
                ImGui::TextColored(GameStyle::kTextMuted, "No entity selected");
            }
        }
        ImGui::End();
    }
}
