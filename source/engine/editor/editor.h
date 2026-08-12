#pragma once
// ---------------------------------------------------------------------------
// editor/editor.h
//
// DTEditor - the top-level class that owns all panels and drives the ImGui
// DockSpace layout.  Inspired by Lacrima Editor pattern.
//
// Usage (from main.cpp):
//   dt::editor::Editor editor;
//   editor.Init(application);     // pass Application& for world/renderer access
//   // Inside the render/ImGui loop:
//   editor.Construct();
//   // On shutdown:
//   editor.Shutdown();
// ---------------------------------------------------------------------------

#include "editor/core/EditorContext.h"

#include <memory>
#include <vector>

namespace dt          { class Application; }
namespace dt::sim     { class SimulationWorld; }
namespace dt::editor  { class EditorPanel; }

namespace dt::editor
{
    class Editor
    {
    public:
        Editor();
        ~Editor();

        Editor(const Editor&)            = delete;
        Editor& operator=(const Editor&) = delete;

        // Must be called after ImGui and Vulkan are initialized.
        void Init(dt::sim::SimulationWorld* world);

        // Called every frame inside the ImGui render pass to draw all panels.
        void Construct();

        // Called before ImGui/Vulkan shutdown.
        void Shutdown();

        EditorContext& Context() { return m_ctx; }

    private:
        void SetupStyle();
        void BuildDockSpace();
        void DrawMenuBar();

        EditorContext                              m_ctx;
        std::vector<std::shared_ptr<EditorPanel>>  m_panels;
        bool                                       m_initialized = false;
        bool                                       m_dockLayoutBuilt = false;
    };
}
