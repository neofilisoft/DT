// Copyright Neofilisoft. All Rights Reserved.
#include "editor/texture/ContentBrowser.h"
#include "core/filesystem/FileSystem.h"
#include "editor/core/EditorContext.h"

#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace lacrima::editor
{
    ContentBrowser::ContentBrowser()
        : EditorPanel("Content Browser")
    {
    }

    void ContentBrowser::Init(EditorContext& ctx)
    {
        (void)ctx;
        std::string assetDir = FileSystem::GetEngineAssetDir();
        if (!assetDir.empty() && std::filesystem::exists(assetDir))
        {
            m_rootPath = assetDir;
        }
        else if (!FileSystem::Get().GetContentRoot().empty() && std::filesystem::exists(FileSystem::Get().GetContentRoot()))
        {
            m_rootPath = FileSystem::Get().GetContentRoot();
        }
        else if (std::filesystem::exists(std::filesystem::current_path() / "assets"))
        {
            m_rootPath = std::filesystem::current_path() / "assets";
        }
        else
        {
            m_rootPath = std::filesystem::current_path();
        }

        NavigateTo(m_rootPath);
    }

    void ContentBrowser::SetRootPath(const std::filesystem::path& dir)
    {
        m_rootPath = dir;
        NavigateTo(dir);
    }

    void ContentBrowser::NavigateTo(const std::filesystem::path& dir)
    {
        m_currentPath = dir;
        RefreshEntries();
    }

    void ContentBrowser::RefreshEntries()
    {
        m_entries.clear();

        if (!std::filesystem::exists(m_currentPath)) return;

        for (const auto& entry : std::filesystem::directory_iterator(m_currentPath))
        {
            ContentEntry ce;
            ce.fullPath    = entry.path();
            ce.name        = entry.path().filename().string();
            ce.isDirectory = entry.is_directory();
            if (!ce.isDirectory)
            {
                ce.extension = entry.path().extension().string();
                // Lowercase the extension for reliable comparison
                std::transform(ce.extension.begin(), ce.extension.end(),
                               ce.extension.begin(), ::tolower);
            }
            m_entries.push_back(std::move(ce));
        }

        // Directories first, then files, alphabetically
        std::stable_sort(m_entries.begin(), m_entries.end(),
                         [](const ContentEntry& a, const ContentEntry& b)
                         {
                             if (a.isDirectory != b.isDirectory)
                                 return a.isDirectory > b.isDirectory;
                             return a.name < b.name;
                         });
    }

    void ContentBrowser::Construct(EditorContext& ctx)
    {
        ImGui::Begin("Content Browser", &m_isOpen);

        DrawBreadcrumb();

        // Refresh button
        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh"))
            RefreshEntries();

        ImGui::Separator();

        // Icon grid
        float panelWidth = ImGui::GetContentRegionAvail().x;
        float itemWidth  = (m_iconSize > 16.0f) ? (m_iconSize + 16.0f) : 88.0f;
        int   columns    = (panelWidth > itemWidth) ? static_cast<int>(panelWidth / itemWidth) : 1;
        if (columns < 1) columns = 1;

        if (ImGui::BeginTable("##content", columns))
        {
            int col = 0;
            for (const auto& entry : m_entries)
            {
                if (col == 0)
                    ImGui::TableNextRow();
                ImGui::TableNextColumn();
                DrawEntry(ctx, entry);
                col = (col + 1) % columns;
            }
            ImGui::EndTable();
        }

        ImGui::End();
    }

    void ContentBrowser::DrawBreadcrumb()
    {
        // Build relative path from root
        auto rel = std::filesystem::relative(m_currentPath, m_rootPath.parent_path());
        std::filesystem::path accumulated = m_rootPath.parent_path();

        for (const auto& part : rel)
        {
            accumulated /= part;
            std::string partStr = part.string();
            ImGui::SameLine(0, 4);
            if (ImGui::SmallButton(partStr.c_str()))
                NavigateTo(accumulated);
            ImGui::SameLine(0, 2);
            ImGui::TextDisabled("/");
        }
    }

    void ContentBrowser::DrawEntry(EditorContext& ctx, const ContentEntry& entry)
    {
        (void)ctx;

        // Choose icon text by type
        const char* icon = entry.isDirectory ? "[DIR]"
                         : (entry.extension == ".asset") ? "[AST]"
                         : (entry.extension == ".lua")   ? "[LUA]"
                         : (entry.extension == ".png" || entry.extension == ".jpg") ? "[IMG]"
                         : (entry.extension == ".glb" || entry.extension == ".gltf") ? "[MES]"
                         : "[FIL]";

        bool isSelected = (m_selectedFile == entry.fullPath.string());

        ImGui::PushID(entry.name.c_str());

        // Render as a selectable button
        if (ImGui::Selectable(icon, isSelected,
                              ImGuiSelectableFlags_AllowDoubleClick,
                              ImVec2(m_iconSize, m_iconSize)))
        {
            m_selectedFile = entry.fullPath.string();
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (entry.isDirectory)
                    NavigateTo(entry.fullPath);
            }
        }

        // Drag & Drop Source
        if (!entry.isDirectory)
        {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                std::string pathStr = entry.fullPath.string();
                ImGui::SetDragDropPayload("DND_ASSET_GLB", pathStr.c_str(), pathStr.size() + 1); // We can just use one payload name for simplicity or differentiate.
                ImGui::Text("Dragging %s", entry.name.c_str());
                ImGui::EndDragDropSource();
            }
        }

        // Label below icon
        ImGui::TextWrapped("%s", entry.name.c_str());

        ImGui::PopID();
    }
}

