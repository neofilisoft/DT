#pragma once

#include "editor/core/EditorContext.h"
#include "editor/home/ProjectHome.h"

#include <memory>
#include <vector>
#include <string>

namespace lacrima          { class Application; }
namespace lacrima::sim     { class SimulationWorld; }
namespace lacrima::editor  { class EditorPanel; }

namespace lacrima::editor
{
    class Editor
    {
    public:
        Editor();
        ~Editor();

        Editor(const Editor&)            = delete;
        Editor& operator=(const Editor&) = delete;

        void Init(lacrima::sim::SimulationWorld* world);
        void Construct();
        void Shutdown();
        void OnDropFile(const std::string& path);

        EditorContext& Context() { return m_ctx; }
        const EditorContext& Context() const { return m_ctx; }

        void SetSceneTexture(void* desc) { m_ctx.SetSceneTexture(desc); }
        bool ConsumeViewportResizeRequest(u32& outW, u32& outH) { return m_ctx.ConsumeViewportResizeRequest(outW, outH); }
        EditorCamera& Camera() { return m_ctx.Camera(); }
        const EditorCamera& Camera() const { return m_ctx.Camera(); }

    private:
        void SetupStyle();
        void BuildDockSpace();
        void DrawMenuBar();

        EditorContext                              m_ctx;
        ProjectHome                                m_home;
        std::vector<std::shared_ptr<EditorPanel>>  m_panels;
        bool                                       m_initialized = false;
        bool                                       m_projectLoaded = false;
        bool                                       m_dockLayoutBuilt = false;
        bool                                       m_styleInitialized = false;
    };
}
