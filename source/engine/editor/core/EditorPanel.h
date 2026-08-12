#pragma once
// ---------------------------------------------------------------------------
// editor/core/EditorPanel.h
//
// Base class for every dockable panel in the DTEditor UI.
// All panels follow Lacrima's EditorUI pattern: init -> construct -> shutdown.
// construct() is called every frame inside the ImGui DockSpace.
// ---------------------------------------------------------------------------

#include <string>

namespace dt::editor
{
    class EditorContext;

    class EditorPanel
    {
    public:
        explicit EditorPanel(const std::string& title) : m_title(title) {}
        virtual ~EditorPanel() = default;

        EditorPanel(const EditorPanel&)            = delete;
        EditorPanel& operator=(const EditorPanel&) = delete;

        // Called once at startup to register event listeners, load icons, etc.
        virtual void Init(EditorContext& ctx) { (void)ctx; }

        // Called every frame to draw the ImGui window.
        virtual void Construct(EditorContext& ctx) = 0;

        // Called once at shutdown to free GPU resources.
        virtual void Shutdown() {}

        const std::string& Title() const { return m_title; }
        bool&              IsOpen()      { return m_isOpen; }

    protected:
        std::string m_title;
        bool        m_isOpen = true;
    };
}
