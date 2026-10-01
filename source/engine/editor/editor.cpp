// Copyright Neofilisoft. All Rights Reserved.
#include "editor/editor.h"
#include "simulation/world/SimulationWorld.h"
#include "core/filesystem/FileSystem.h"
#include <imgui_internal.h>

// Core
#include "editor/core/EditorPanel.h"
// Panels
#include "editor/scene/SceneOutliner.h"
#include "editor/scene/PropertyInspector.h"
#include "editor/scene/ViewportPanel.h"
#include "editor/scene/ToolbarPanel.h"
#include "editor/texture/ContentBrowser.h"
#include "editor/profiling/LogConsole.h"
#include "editor/export/BuildTool.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>

namespace lacrima::editor
{
    Editor::Editor() = default;
    Editor::~Editor() { Shutdown(); }

    void Editor::Init(lacrima::sim::SimulationWorld* world)
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
        addPanel(std::make_shared<ToolbarPanel>());
        addPanel(std::make_shared<ContentBrowser>());
        addPanel(std::make_shared<LogConsole>());
        addPanel(std::make_shared<BuildTool>());

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

        if (!m_styleInitialized)
        {
            SetupStyle();
            m_styleInitialized = true;
        }

        if (!m_projectLoaded)
        {
            m_home.Init();
            if (m_home.Construct(m_ctx))
            {
                m_projectLoaded = true;
                const std::string& projPath = m_home.GetCurrentProjectPath();
                if (!projPath.empty())
                {
                    std::filesystem::path p(projPath);
                    std::filesystem::path assetsPath = p / "assets";
                    if (std::filesystem::exists(assetsPath))
                    {
                        lacrima::FileSystem::Get().SetContentRoot(assetsPath.string());
                    }
                    else
                    {
                        lacrima::FileSystem::Get().SetContentRoot(p.string());
                    }

                    // Refresh Content Browser to project assets
                    for (auto& panel : m_panels)
                    {
                        if (panel->Title() == "Content Browser")
                        {
                            if (auto cb = std::dynamic_pointer_cast<ContentBrowser>(panel))
                            {
                                cb->SetRootPath(lacrima::FileSystem::Get().GetContentRoot());
                            }
                        }
                    }
                }
            }
            return;
        }

        BuildDockSpace();

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
                if (ImGui::MenuItem("New Scene", "Ctrl+N"))  {}
                if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {}
                if (ImGui::MenuItem("Save", "Ctrl+S")) {}
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("New Project")) { m_projectLoaded = false; }
                if (ImGui::MenuItem("Open Project")) { m_projectLoaded = false; }
                if (ImGui::MenuItem("Save Project")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Exit"))
                {
                    SDL_Event quitEvent{};
                    quitEvent.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quitEvent);
                }
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

            if (ImGui::BeginMenu("Project")) { ImGui::EndMenu(); }
            if (ImGui::BeginMenu("Component")) { ImGui::EndMenu(); }
            if (ImGui::BeginMenu("Jobs")) { ImGui::EndMenu(); }

            if (ImGui::BeginMenu("Window"))
            {
                for (auto& panel : m_panels)
                {
                    ImGui::MenuItem(panel->Title().c_str(), nullptr, &panel->IsOpen());
                }
                ImGui::Separator();
                bool fullscreen = m_ctx.IsViewportFullscreen();
                if (ImGui::MenuItem("Viewport Fullscreen", "F11", fullscreen))
                    m_ctx.ToggleViewportFullscreen();
                if (ImGui::MenuItem("Reset Layout"))
                    m_dockLayoutBuilt = false;
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

            if (ImGui::BeginMenu("Help")) { ImGui::EndMenu(); }

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
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("LacrimaEditorDockSpace", nullptr, window_flags);

        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

            if (!m_dockLayoutBuilt)
            {
                m_dockLayoutBuilt = true;
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

                ImGuiID dock_main_id = dockspace_id;
                ImGuiID dock_left_id  = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left,  0.20f, nullptr, &dock_main_id);
                ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
                ImGuiID dock_down_id  = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down,  0.30f, nullptr, &dock_main_id);
                ImGuiID dock_up_id    = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up,    0.05f, nullptr, &dock_main_id);

                // Dock windows using EXACT titles declared in panel ImGui::Begin calls
                ImGui::DockBuilderDockWindow("Outliner", dock_left_id);
                ImGui::DockBuilderDockWindow("Inspector", dock_right_id);
                ImGui::DockBuilderDockWindow("Content Browser", dock_down_id);
                ImGui::DockBuilderDockWindow("Console", dock_down_id);
                ImGui::DockBuilderDockWindow("Build Tool", dock_down_id);
                ImGui::DockBuilderDockWindow("Toolbar", dock_up_id);
                ImGui::DockBuilderDockWindow("Viewport", dock_main_id);

                ImGui::DockBuilderFinish(dockspace_id);
            }
        }

        DrawMenuBar();

        ImGui::End();
    }

    void Editor::Shutdown()
    {
        if (!m_initialized) return;
        for (auto& panel : m_panels)
            panel->Shutdown();
        m_panels.clear();
        m_initialized = false;
    }

    void Editor::OnDropFile(const std::string& path)
    {
        LACRIMA_LOG_INFO(LogCategory::Core, "Editor: File dropped: %s", path.c_str());

        try {
            std::filesystem::path srcPath(path);
            if (!std::filesystem::exists(srcPath)) return;

            // Copy to asset directory
            std::filesystem::path assetDir = std::filesystem::current_path() / "source" / "engine" / "asset";
            if (!std::filesystem::exists(assetDir))
            {
                std::filesystem::create_directories(assetDir);
            }

            std::filesystem::path destPath = assetDir / srcPath.filename();
            std::filesystem::copy_file(srcPath, destPath, std::filesystem::copy_options::overwrite_existing);

            LACRIMA_LOG_INFO(LogCategory::Core, "Editor: Successfully imported asset to %s", destPath.string().c_str());

            // Find ContentBrowser and refresh it
            for (auto& panel : m_panels)
            {
                if (panel->Title() == "Content Browser")
                {
                    if (auto cb = std::dynamic_pointer_cast<ContentBrowser>(panel))
                    {
                        cb->RefreshEntries();
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "Editor: Failed to import dropped file: %s", e.what());
        }
    }
}








