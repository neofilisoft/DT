// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "editor/core/EditorPanel.h"

namespace lacrima::editor
{
    class ToolbarPanel : public EditorPanel
    {
    public:
        ToolbarPanel() : EditorPanel("Toolbar") {}
        ~ToolbarPanel() override = default;

        void Init(EditorContext& ctx) override;
        void Construct(EditorContext& ctx) override;
    };
}

