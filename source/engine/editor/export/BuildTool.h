#pragma once
// ---------------------------------------------------------------------------
// editor/export/BuildTool.h
//
// Editor-integrated Build/Export pipeline.
// Provides a UI panel and backend logic to:
//   1. Cook all assets (invoke DTCooker for every source file in content/).
//   2. Package the game binary + assets into a distributable folder.
//   3. Select target platform (Windows .exe / Linux AppImage / Web wasm).
//
// The actual compilation is handled by invoking cmake + cmake --install via
// std::system / CreateProcess.  This avoids re-implementing build logic inside
// the editor; it just orchestrates the existing build system.
// ---------------------------------------------------------------------------

#include "editor/core/EditorPanel.h"
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>

namespace dt::editor
{
    enum class TargetPlatform
    {
        WindowsX64,    // .exe
        LinuxX64,      // AppImage / .deb
        Web,           // Emscripten WASM
    };

    struct BuildConfig
    {
        TargetPlatform platform    = TargetPlatform::WindowsX64;
        bool           debugBuild  = false;
        bool           cookAssets  = true;
        std::string    outputDir   = "dist";
        std::string    projectRoot = ".";
    };

    class BuildTool final : public EditorPanel
    {
    public:
        BuildTool();

        void Init(EditorContext& ctx) override;
        void Construct(EditorContext& ctx) override;
        void Shutdown() override;

    private:
        void StartBuild();
        void CancelBuild();
        bool IsBuildRunning() const { return m_buildRunning.load(); }

        void RunBuildThread();

        BuildConfig             m_config;
        std::atomic<bool>       m_buildRunning{ false };
        std::thread             m_buildThread;
        std::vector<std::string> m_buildLog;
        bool                    m_buildSucceeded = false;
        bool                    m_autoScroll     = true;
    };
}
