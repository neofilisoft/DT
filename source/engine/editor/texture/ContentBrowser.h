#pragma once
// ---------------------------------------------------------------------------
// editor/texture/ContentBrowser.h (lives in texture/ folder per user layout)
//
// Browses the project content directory showing .asset, .png, .lua, .glb
// files with icon thumbnails. Double-click triggers the asset pipeline
// (invokes DTCooker for raw source files).
// ---------------------------------------------------------------------------

#include "editor/core/EditorPanel.h"
#include <string>
#include <vector>
#include <filesystem>

namespace dt::editor
{
    struct ContentEntry
    {
        std::string              name;
        std::filesystem::path    fullPath;
        bool                     isDirectory = false;
        std::string              extension;  // lowercase .asset / .png / .lua ...
    };

    class ContentBrowser final : public EditorPanel
    {
    public:
        ContentBrowser();

        void Init(EditorContext& ctx) override;
        void Construct(EditorContext& ctx) override;

    private:
        void NavigateTo(const std::filesystem::path& dir);
        void RefreshEntries();
        void DrawBreadcrumb();
        void DrawEntry(EditorContext& ctx, const ContentEntry& entry);

        std::filesystem::path        m_rootPath;
        std::filesystem::path        m_currentPath;
        std::vector<ContentEntry>    m_entries;
        std::string                  m_selectedFile;

        float m_iconSize = 72.0f;
    };
}
