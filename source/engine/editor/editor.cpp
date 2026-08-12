#include "editor/editor.h"

// Core
#include "editor/core/EditorPanel.h"
// Panels
#include "editor/scene/SceneOutliner.h"
#include "editor/scene/PropertyInspector.h"
#include "editor/scene/ViewportPanel.h"
#include "editor/texture/ContentBrowser.h"
#include "editor/profiling/LogConsole.h"
#include "editor/export/BuildTool.h"

#include <imgui.h>

namespace dt::editor
{
    Editor::Editor() = default;
    Editor::~Editor() { Shutdown(); }

    void Editor::Init(dt::sim::SimulationWorld* world)
    {
        if (m_initialized) return;

        m_ctx.SetWorld(world);

        // Register all panels
        auto addPanel = [&](std::shared_ptr<EditorPanel> p)
        {
            p->Init(m_ctx);
            m_panels.push_back(std::move(p));
        };

        addPanel(std::make_shared<SceneOutliner>());
        addPanel(std::make_shared<PropertyInspector>());
        addPanel(std::make_shared<ViewportPanel>());
        addPanel(std::make_shared<ContentBrowser>());
        addPanel(std::make_shared<LogConsole>());
        addPanel(std::make_shared<BuildTool>());

        SetupStyle();
        m_initialized = true;
    }

    void Editor::SetupStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        // Dark slate theme inspired by Unreal Engine 5
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg]         = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_Header]           = ImVec4(0.20f, 0.20f, 0.26f, 1.00f);
        colors[ImGuiCol_HeaderHovered]    = ImVec4(0.26f, 0.26f, 0.34f, 1.00f);
        colors[ImGuiCol_HeaderActive]     = ImVec4(0.26f, 0.60f, 0.90f, 1.00f);
        colors[ImGuiCol_Button]           = ImVec4(0.22f, 0.22f, 0.28f, 1.00f);
        colors[ImGuiCol_ButtonHovered]    = ImVec4(0.30f, 0.60f, 0.90f, 1.00f);
        colors[ImGuiCol_ButtonActive]     = ImVec4(0.20f, 0.50f, 0.80f, 1.00f);
        colors[ImGuiCol_TitleBgActive]    = ImVec4(0.13f, 0.13f, 0.17f, 1.00f);
        colors[ImGuiCol_Tab]              = ImVec4(0.14f, 0.14f, 0.19f, 1.00f);
        colors[ImGuiCol_TabHovered]       = ImVec4(0.26f, 0.60f, 0.90f, 1.00f);
        colors[ImGuiCol_TabActive]        = ImVec4(0.20f, 0.45f, 0.76f, 1.00f);
        colors[ImGuiCol_Separator]        = ImVec4(0.28f, 0.28f, 0.35f, 1.00f);
        colors[ImGuiCol_FrameBg]          = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
        colors[ImGuiCol_CheckMark]        = ImVec4(0.26f, 0.60f, 0.90f, 1.00f);

        style.WindowRounding    = 4.0f;
        style.FrameRounding     = 3.0f;
        style.TabRounding       = 3.0f;
        style.ScrollbarRounding = 3.0f;
        style.FramePadding      = ImVec2(6, 4);
        style.ItemSpacing       = ImVec2(8, 5);
    }

    void Editor::Construct()
    {
        if (!m_initialized) return;

        DrawMenuBar();

        // Draw all panels unless viewport is fullscreen
        if (m_ctx.IsViewportFullscreen())
        {
            // Only draw viewport in fullscreen mode
            for (auto& panel : m_panels)
            {
                if (panel->Title() == "Viewport")
                    panel->Construct(m_ctx);
            }
        }
        else
        {
            for (auto& panel : m_panels)
            {
                if (panel->IsOpen())
                    panel->Construct(m_ctx);
            }
        }
    }

    void Editor::DrawMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Scene"))  {}
                if (ImGui::MenuItem("Open Scene")) {}
                if (ImGui::MenuItem("Save Scene")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Exit"))
                    m_ctx.SetMode(EditorMode::Edit); // placeholder - wire SDL quit
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_ctx.CanUndo()))
                    m_ctx.Undo();
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_ctx.CanRedo()))
                    m_ctx.Redo();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                for (auto& panel : m_panels)
                {
                    ImGui::MenuItem(panel->Title().c_str(), nullptr, &panel->IsOpen());
                }
                ImGui::Separator();
                bool fullscreen = m_ctx.IsViewportFullscreen();
                if (ImGui::MenuItem("Viewport Fullscreen", "F11", fullscreen))
                    m_ctx.ToggleViewportFullscreen();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Simulation"))
            {
                if (ImGui::MenuItem("[>] Play",  "F5",  m_ctx.IsPlaying()))
                    m_ctx.SetMode(EditorMode::Play);
                if (ImGui::MenuItem("[sq] Stop", "F6",  !m_ctx.IsPlaying()))
                    m_ctx.SetMode(EditorMode::Edit);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Build"))
            {
                for (auto& panel : m_panels)
                {
                    if (panel->Title() == "Build Tool")
                    {
                        if (ImGui::MenuItem("Open Build Tool"))
                            panel->IsOpen() = true;
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        // Keyboard shortcuts
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) m_ctx.Undo();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) m_ctx.Redo();
        if (ImGui::IsKeyPressed(ImGuiKey_F11))              m_ctx.ToggleViewportFullscreen();
    }

    void Editor::BuildDockSpace()
    {
        // No-op for non-docking imgui version
    }

    void Editor::Shutdown()
    {
        if (!m_initialized) return;
        for (auto& panel : m_panels)
            panel->Shutdown();
        m_panels.clear();
        m_initialized = false;
    }
}
