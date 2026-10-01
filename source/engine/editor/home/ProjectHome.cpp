// Copyright Neofilisoft. All Rights Reserved.
#include "editor/home/ProjectHome.h"
#include "editor/core/EditorContext.h"
#include "core/logging/Logger.h"
#include "core/filesystem/FileSystem.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

static bool OpenNativeFileDialog(std::string& outPath)
{
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "Lacrima Project (*.dtproj)\0*.dtproj\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn))
    {
        outPath = filename;
        return true;
    }
    return false;
}
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace lacrima::editor
{
    static std::string GetDefaultProjectsDirectory()
    {
        std::string defaultPath;
#ifdef _WIN32
        const char* userProfile = std::getenv("USERPROFILE");
        if (userProfile && *userProfile)
        {
            defaultPath = (std::filesystem::path(userProfile) / "Documents" / "DTProjects").string();
        }
#else
        const char* home = std::getenv("HOME");
        if (home && *home)
        {
            defaultPath = (std::filesystem::path(home) / "DTProjects").string();
        }
#endif
        if (defaultPath.empty())
        {
            std::string exeDir = FileSystem::GetExecutableDir();
            if (!exeDir.empty())
                defaultPath = (std::filesystem::path(exeDir) / "projects").string();
            else
                defaultPath = (std::filesystem::current_path() / "projects").string();
        }
        return FileSystem::NormalizeSeparators(defaultPath);
    }

    static std::string GetEditorSettingsPath()
    {
        std::string exeDir = FileSystem::GetExecutableDir();
        if (!exeDir.empty())
            return FileSystem::NormalizeSeparators(exeDir + "/editor_settings.json");
        return FileSystem::NormalizeSeparators((std::filesystem::current_path() / "editor_settings.json").string());
    }

    void ProjectHome::Init()
    {
        if (m_isLoaded) return;
        LoadRecentProjects();
        m_isLoaded = true;
    }

    void ProjectHome::LoadRecentProjects()
    {
        m_recentProjects.clear();
        
        fs::path settingsPath = GetEditorSettingsPath();
        if (fs::exists(settingsPath))
        {
            try
            {
                std::ifstream f(settingsPath);
                json data = json::parse(f);
                
                if (data.contains("recent_projects"))
                {
                    for (const auto& item : data["recent_projects"])
                    {
                        m_recentProjects.push_back({
                            item["name"].get<std::string>(),
                            item["path"].get<std::string>(),
                            item["lastOpened"].get<std::string>()
                        });
                    }
                }
            }
            catch (const std::exception& e)
            {
                LACRIMA_LOG_ERROR(lacrima::LogCategory::Core, "Failed to load editor_settings.json: %s", e.what());
            }
        }
    }

    void ProjectHome::SaveRecentProjects()
    {
        json data;
        data["recent_projects"] = json::array();
        
        for (const auto& proj : m_recentProjects)
        {
            data["recent_projects"].push_back({
                {"name", proj.name},
                {"path", proj.path},
                {"lastOpened", proj.lastOpened}
            });
        }
        
        try
        {
            fs::path settingsPath = GetEditorSettingsPath();
            std::ofstream f(settingsPath);
            f << data.dump(4);
        }
        catch (const std::exception& e)
        {
            LACRIMA_LOG_ERROR(lacrima::LogCategory::Core, "Failed to save editor_settings.json: %s", e.what());
        }
    }

    void ProjectHome::AddProject(const std::string& path, const std::string& name)
    {
        std::string normPath = FileSystem::NormalizeSeparators(path);
        for (auto it = m_recentProjects.begin(); it != m_recentProjects.end(); ++it)
        {
            if (FileSystem::NormalizeSeparators(it->path) == normPath)
            {
                it->lastOpened = "Just now";
                it->name = name;
                ProjectInfo proj = *it;
                m_recentProjects.erase(it);
                m_recentProjects.insert(m_recentProjects.begin(), proj);
                SaveRecentProjects();
                return;
            }
        }
        m_recentProjects.insert(m_recentProjects.begin(), {name, normPath, "Just now"});
        SaveRecentProjects();
    }

    void ProjectHome::RemoveProject(size_t index)
    {
        if (index < m_recentProjects.size())
        {
            m_recentProjects.erase(m_recentProjects.begin() + index);
            SaveRecentProjects();
        }
    }

    void ProjectHome::RenameProject(size_t index, const std::string& newName)
    {
        if (index < m_recentProjects.size())
        {
            auto& proj = m_recentProjects[index];
            std::string oldPathStr = proj.path;
            fs::path oldPath = fs::path(oldPathStr);
            fs::path newPath = oldPath.parent_path() / newName;

            // Rename directory if it exists and hasn't just changed case
            if (fs::exists(oldPath) && oldPath != newPath)
            {
                bool dirRenamed = false;
                try
                {
                    fs::rename(oldPath, newPath);
                    dirRenamed = true;
                    
                    // Rename the .dtproj file inside
                    fs::path oldProjFile = newPath / (proj.name + ".dtproj");
                    fs::path newProjFile = newPath / (newName + ".dtproj");
                    if (fs::exists(oldProjFile))
                    {
                        fs::rename(oldProjFile, newProjFile);
                        // Also update the JSON inside
                        std::ifstream in(newProjFile);
                        if (in.is_open()) {
                            json projData = json::parse(in);
                            in.close();
                            projData["name"] = newName;
                            std::ofstream out(newProjFile);
                            out << projData.dump(4) << "\n";
                        }
                    }
                    
                    proj.path = newPath.string();
                    proj.name = newName;
                }
                catch (const std::exception& e)
                {
                    LACRIMA_LOG_ERROR(lacrima::LogCategory::Core, "Failed to rename workspace: %s", e.what());
                    
                    if (dirRenamed)
                    {
                        try { fs::rename(newPath, oldPath); }
                        catch (...) { LACRIMA_LOG_ERROR(lacrima::LogCategory::Core, "CRITICAL: Rollback failed for workspace rename!"); }
                    }
                    return;
                }
            }
            else
            {
                proj.name = newName;
            }
            
            SaveRecentProjects();
        }
    }

    void ProjectHome::CreateNewProject(const std::string& directory, const std::string& name)
    {
        fs::path projPath = fs::path(directory) / name;
        if (!fs::exists(projPath))
        {
            fs::create_directories(projPath);
            fs::create_directories(projPath / "assets");
            fs::create_directories(projPath / "source");
            fs::create_directories(projPath / "scenes");
            
            // Create a simple project file using json
            json projData;
            projData["name"] = name;
            projData["version"] = "1.0";
            
            std::ofstream out(projPath / (name + ".dtproj"));
            out << projData.dump(4) << "\n";
        }
        
        m_currentProjectPath = FileSystem::NormalizeSeparators(projPath.string());
        m_currentProjectName = name;
        AddProject(m_currentProjectPath, name);
    }

    bool ProjectHome::OpenProject(const std::string& pathOrFile)
    {
        if (pathOrFile.empty())
        {
            return false;
        }

        try
        {
            fs::path p(pathOrFile);
            if (!fs::exists(p))
            {
                LACRIMA_LOG_ERROR(lacrima::LogCategory::Core, "ProjectHome: Path does not exist: %s", pathOrFile.c_str());
                return false;
            }

            fs::path projDir;
            std::string projName;

            if (fs::is_regular_file(p))
            {
                projDir = p.parent_path();
                try
                {
                    std::ifstream in(p);
                    if (in.is_open())
                    {
                        json projData = json::parse(in);
                        if (projData.contains("name"))
                        {
                            projName = projData["name"].get<std::string>();
                        }
                    }
                }
                catch (...) {}

                if (projName.empty())
                {
                    projName = p.stem().string();
                }
            }
            else if (fs::is_directory(p))
            {
                projDir = p;
                for (const auto& entry : fs::directory_iterator(p))
                {
                    if (entry.is_regular_file() && entry.path().extension() == ".dtproj")
                    {
                        try
                        {
                            std::ifstream in(entry.path());
                            if (in.is_open())
                            {
                                json projData = json::parse(in);
                                if (projData.contains("name"))
                                {
                                    projName = projData["name"].get<std::string>();
                                    break;
                                }
                            }
                        }
                        catch (...) {}
                    }
                }

                if (projName.empty())
                {
                    projName = p.filename().string();
                }
            }
            else
            {
                return false;
            }

            m_currentProjectPath = FileSystem::NormalizeSeparators(projDir.string());
            m_currentProjectName = projName;

            AddProject(m_currentProjectPath, m_currentProjectName);
            LACRIMA_LOG_INFO(lacrima::LogCategory::Core, "ProjectHome: Loaded project '%s' at: %s",
                m_currentProjectName.c_str(), m_currentProjectPath.c_str());
            return true;
        }
        catch (const std::exception& e)
        {
            LACRIMA_LOG_ERROR(lacrima::LogCategory::Core, "ProjectHome: Failed to open project: %s", e.what());
            return false;
        }
    }

    bool ProjectHome::Construct(EditorContext& ctx)
    {
        bool projectSelected = false;

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("ProjectHomeHub", nullptr, flags))
        {
            ImGui::Text("Lacrima Hub - Projects");
            ImGui::Separator();

            if (ImGui::BeginTable("HubTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 250.0f);
                ImGui::TableSetupColumn("RecentProjects", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextColumn();

                // Left Panel (Buttons)
                if (ImGui::Button("New Project", ImVec2(-1, 40)))
                {
                    m_showNewProjectModal = true;
                    strcpy(m_newProjectName, "MyGame");
                    std::string defPath = GetDefaultProjectsDirectory();
                    strncpy(m_newProjectPath, defPath.c_str(), sizeof(m_newProjectPath) - 1);
                    m_newProjectPath[sizeof(m_newProjectPath) - 1] = '\0';
                }

                if (ImGui::Button("Open Project", ImVec2(-1, 40)))
                {
                    bool opened = false;
#ifdef _WIN32
                    std::string selectedFile;
                    if (OpenNativeFileDialog(selectedFile))
                    {
                        if (OpenProject(selectedFile))
                        {
                            projectSelected = true;
                            opened = true;
                        }
                    }
#endif
                    if (!opened)
                    {
                        m_showOpenProjectModal = true;
                        std::string defPath = GetDefaultProjectsDirectory();
                        strncpy(m_openProjectPath, defPath.c_str(), sizeof(m_openProjectPath) - 1);
                        m_openProjectPath[sizeof(m_openProjectPath) - 1] = '\0';
                        m_openProjectError.clear();
                    }
                }

                ImGui::TableNextColumn();

                // Right Panel (Recent Projects)
                ImGui::Text("Recent Projects");
                ImGui::Separator();

                if (m_recentProjects.empty())
                {
                    ImGui::TextDisabled("No recent projects found.");
                }
                else
                {
                    for (size_t i = 0; i < m_recentProjects.size(); ++i)
                    {
                        const auto& proj = m_recentProjects[i];
                        ImGui::PushID(static_cast<int>(i));

                        bool isSelected = (m_selectedRecentIndex == static_cast<int>(i));
                        if (ImGui::Selectable(proj.name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(-1, 50)))
                        {
                            m_selectedRecentIndex = static_cast<int>(i);
                            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            {
                                if (OpenProject(proj.path))
                                {
                                    projectSelected = true;
                                }
                            }
                        }

                        if (ImGui::BeginPopupContextItem("ProjectContextMenu"))
                        {
                            if (ImGui::MenuItem("Open"))
                            {
                                if (OpenProject(proj.path))
                                {
                                    projectSelected = true;
                                }
                            }
                            if (ImGui::MenuItem("Remove from list"))
                            {
                                RemoveProject(i);
                            }
                            if (ImGui::MenuItem("Rename"))
                            {
                                m_renameTargetIndex = i;
                                strcpy(m_renameProjectName, proj.name.c_str());
                                m_showRenameModal = true;
                            }
                            if (ImGui::MenuItem("Show in Explorer"))
                            {
#ifdef _WIN32
                                ShellExecuteA(NULL, "open", proj.path.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
                            }
                            ImGui::EndPopup();
                        }

                        ImGui::TextDisabled("%s", proj.path.c_str());
                        ImGui::PopID();
                        ImGui::Separator();
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();

        // Modals
        if (m_showNewProjectModal)
        {
            ImGui::OpenPopup("Create New Project");
            m_showNewProjectModal = false;
        }

        if (ImGui::BeginPopupModal("Create New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Project Name", m_newProjectName, sizeof(m_newProjectName));
            ImGui::InputText("Location", m_newProjectPath, sizeof(m_newProjectPath));
            
            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                CreateNewProject(m_newProjectPath, m_newProjectName);
                ImGui::CloseCurrentPopup();
                projectSelected = true;
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (m_showOpenProjectModal)
        {
            ImGui::OpenPopup("Open Project");
            m_showOpenProjectModal = false;
        }

        if (ImGui::BeginPopupModal("Open Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Enter project directory or select a .dtproj file:");
            ImGui::InputText("Project Path", m_openProjectPath, sizeof(m_openProjectPath));
            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
#ifdef _WIN32
                std::string browsed;
                if (OpenNativeFileDialog(browsed))
                {
                    strncpy(m_openProjectPath, browsed.c_str(), sizeof(m_openProjectPath) - 1);
                    m_openProjectPath[sizeof(m_openProjectPath) - 1] = '\0';
                }
#endif
            }

            if (!m_openProjectError.empty())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_openProjectError.c_str());
            }

            if (ImGui::Button("Open", ImVec2(120, 0)))
            {
                if (OpenProject(m_openProjectPath))
                {
                    ImGui::CloseCurrentPopup();
                    projectSelected = true;
                }
                else
                {
                    m_openProjectError = "Invalid project directory or .dtproj file!";
                }
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (m_showRenameModal)
        {
            ImGui::OpenPopup("Rename");
            m_showRenameModal = false;
        }

        if (ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("New Name", m_renameProjectName, sizeof(m_renameProjectName));
            
            if (ImGui::Button("Rename", ImVec2(120, 0)))
            {
                RenameProject(m_renameTargetIndex, m_renameProjectName);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        return projectSelected;
    }
}
