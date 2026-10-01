// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include <string>
#include <vector>

namespace lacrima::editor
{
    class EditorContext;

    class ProjectHome
    {
    public:
        ProjectHome() = default;
        ~ProjectHome() = default;

        void Init();
        
        // Returns true if a project was selected, false if the Hub is still open.
        bool Construct(EditorContext& ctx);

        const std::string& GetCurrentProjectPath() const { return m_currentProjectPath; }
        const std::string& GetCurrentProjectName() const { return m_currentProjectName; }

        bool OpenProject(const std::string& pathOrFile);

    private:
        struct ProjectInfo
        {
            std::string name;
            std::string path;
            std::string lastOpened;
        };

        void LoadRecentProjects();
        void SaveRecentProjects();
        void AddProject(const std::string& path, const std::string& name);
        void RemoveProject(size_t index);
        void RenameProject(size_t index, const std::string& newName);
        void CreateNewProject(const std::string& directory, const std::string& name);

        std::vector<ProjectInfo> m_recentProjects;
        bool m_isLoaded = false;
        
        std::string m_currentProjectPath;
        std::string m_currentProjectName;

        // Modal state
        bool m_showNewProjectModal = false;
        char m_newProjectName[256] = "";
        char m_newProjectPath[1024] = "";

        bool m_showOpenProjectModal = false;
        char m_openProjectPath[1024] = "";
        std::string m_openProjectError;

        bool m_showRenameModal = false;
        size_t m_renameTargetIndex = 0;
        char m_renameProjectName[256] = "";

        int m_selectedRecentIndex = -1;
    };
}
