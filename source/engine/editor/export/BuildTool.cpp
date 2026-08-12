#include "editor/export/BuildTool.h"
#include "editor/core/EditorContext.h"
#include "core/logging/Logger.h"

#include <imgui.h>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <array>
#include <cstdio>

namespace dt::editor
{
    static std::string ExecCapture(const std::string& cmd)
    {
        std::string result;
#ifdef _WIN32
        FILE* pipe = _popen((cmd + " 2>&1").c_str(), "r");
#else
        FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
#endif
        if (!pipe) return "[error: could not launch process]";

        std::array<char, 256> buf;
        while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe))
            result += buf.data();

#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
        return result;
    }

    BuildTool::BuildTool()
        : EditorPanel("Build Tool")
    {
    }

    void BuildTool::Init(EditorContext& ctx)
    {
        (void)ctx;
        m_config.projectRoot = std::filesystem::current_path().string();
        m_config.outputDir   = (std::filesystem::current_path() / "dist").string();
    }

    void BuildTool::Shutdown()
    {
        CancelBuild();
    }

    void BuildTool::StartBuild()
    {
        if (m_buildRunning.load()) return;
        m_buildLog.clear();
        m_buildSucceeded = false;
        m_buildRunning.store(true);
        m_buildThread = std::thread(&BuildTool::RunBuildThread, this);
    }

    void BuildTool::CancelBuild()
    {
        m_buildRunning.store(false);
        if (m_buildThread.joinable())
            m_buildThread.join();
    }

    void BuildTool::RunBuildThread()
    {
        auto log = [this](const std::string& msg)
        {
            m_buildLog.push_back(msg);
            DT_LOG_INFO(LogCategory::Core, "[BuildTool] {}", msg);
        };

        log("Build started.");

        // Step 1: Cook assets
        if (m_config.cookAssets)
        {
            log("[Cook] Scanning for source assets...");
            namespace fs = std::filesystem;
            std::filesystem::path assetDir = std::filesystem::path(m_config.projectRoot) / "source" / "engine" / "asset";

            if (std::filesystem::exists(assetDir))
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(assetDir))
                {
                    if (!m_buildRunning.load()) { log("[Cook] Cancelled."); return; }

                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    std::string cookerType;
                    if      (ext == ".png" || ext == ".jpg") cookerType = "texture";
                    else if (ext == ".glb" || ext == ".gltf") cookerType = "mesh";
                    else if (ext == ".vert" || ext == ".frag") cookerType = "shader";

                    if (cookerType.empty()) continue;

                    std::filesystem::path inputPath  = entry.path();
                    std::filesystem::path outputPath = inputPath;
                    outputPath.replace_extension(".asset");

                    std::string cmd = "DTCooker -i \"" + inputPath.string()
                                    + "\" -o \"" + outputPath.string()
                                    + "\" -t " + cookerType;
                    log("[Cook] " + inputPath.filename().string());
                    std::string result = ExecCapture(cmd);
                    if (!result.empty())
                        log("  " + result);
                }
            }
            else
            {
                log("[Cook] Asset directory not found: " + assetDir.string());
            }
        }

        if (!m_buildRunning.load()) { log("Cancelled."); return; }

        // Step 2: Configure cmake
        std::string buildType = m_config.debugBuild ? "Debug" : "Release";
        std::string buildDir  = (std::filesystem::path(m_config.projectRoot) / "build_dist").string();

        std::string cmakeConfig = "cmake -S \"" + m_config.projectRoot
                                + "\" -B \"" + buildDir
                                + "\" -DCMAKE_BUILD_TYPE=" + buildType;
        log("[CMake] " + cmakeConfig);
        log(ExecCapture(cmakeConfig));

        if (!m_buildRunning.load()) { log("Cancelled."); return; }

        // Step 3: Build
        std::string cmakeBuild = "cmake --build \"" + buildDir
                               + "\" --config " + buildType + " -- -j4";
        log("[Build] " + cmakeBuild);
        log(ExecCapture(cmakeBuild));

        if (!m_buildRunning.load()) { log("Cancelled."); return; }

        // Step 4: Install / Copy to output dir
        std::filesystem::create_directories(m_config.outputDir);
        std::string cmakeInstall = "cmake --install \"" + buildDir
                                 + "\" --prefix \"" + m_config.outputDir + "\"";
        log("[Install] " + cmakeInstall);
        log(ExecCapture(cmakeInstall));

        m_buildSucceeded = true;
        log("Build complete -> " + m_config.outputDir);
        m_buildRunning.store(false);
    }

    void BuildTool::Construct(EditorContext& ctx)
    {
        (void)ctx;
        ImGui::Begin("Build Tool", &m_isOpen);

        ImGui::SeparatorText("Build Configuration");

        // Platform selector
        const char* platforms[] = { "Windows x64 (.exe)", "Linux x64 (AppImage)", "Web (WASM)" };
        int platformIdx = static_cast<int>(m_config.platform);
        if (ImGui::Combo("Target Platform", &platformIdx, platforms, 3))
            m_config.platform = static_cast<TargetPlatform>(platformIdx);

        ImGui::Checkbox("Debug Build", &m_config.debugBuild);
        ImGui::Checkbox("Cook Assets Before Build", &m_config.cookAssets);

        char outputBuf[512];
        strncpy(outputBuf, m_config.outputDir.c_str(), sizeof(outputBuf) - 1);
        if (ImGui::InputText("Output Directory", outputBuf, sizeof(outputBuf)))
            m_config.outputDir = outputBuf;

        ImGui::Separator();

        bool running = IsBuildRunning();

        if (running)
        {
            ImGui::BeginDisabled();
            ImGui::Button("[Build & Export]");
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) CancelBuild();

            // Spinner animation
            static float spinAngle = 0.0f;
            spinAngle += ImGui::GetIO().DeltaTime * 4.0f;
            ImGui::SameLine();
            ImGui::Text("[...] Building (%.0f)", spinAngle);
        }
        else
        {
            if (ImGui::Button("[Build & Export]"))
                StartBuild();

            if (!m_buildLog.empty())
            {
                ImGui::SameLine();
                ImGui::TextColored(m_buildSucceeded
                                   ? ImVec4(0.2f, 1.0f, 0.3f, 1.0f)
                                   : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                   m_buildSucceeded ? "OK" : "FAILED");
            }
        }

        ImGui::Separator();
        ImGui::SeparatorText("Build Log");

        ImGui::Checkbox("Auto-scroll", &m_autoScroll);
        ImGui::BeginChild("##buildlog", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& line : m_buildLog)
            ImGui::TextUnformatted(line.c_str());

        if (m_autoScroll)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::End();
    }
}
