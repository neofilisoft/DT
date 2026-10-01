// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/asset/AssetManager.h"
#include "core/platform/Types.h"

#include <string>
#include <vector>

namespace lacrima::asset
{
    // Cooked metadata for a sequence of standalone 2D frames. Pixel data
    // remains in texture assets; this asset is the stable game-facing clip
    // contract and keeps source directory layout out of gameplay code.
    class SpriteAnimationAsset final : public IAsset
    {
    public:
        bool LoadFromFile(const std::string& path) override;

        u32 Width() const { return m_width; }
        u32 Height() const { return m_height; }
        f32 FramesPerSecond() const { return m_framesPerSecond; }
        bool IsLooping() const { return m_looping; }
        bool IsPixelPerfect() const { return m_pixelPerfect; }
        u32 AtlasColumns() const { return m_atlasColumns; }
        u32 AtlasRows() const { return m_atlasRows; }
        usize FrameCount() const { return m_frameNames.size(); }
        const std::string& FramePath(usize index) const { return m_frameNames.at(static_cast<size_t>(index)); }

    private:
        u32 m_width = 0;
        u32 m_height = 0;
        f32 m_framesPerSecond = 1.0f;
        bool m_looping = true;
        bool m_pixelPerfect = true;
        u32 m_atlasColumns = 1;
        u32 m_atlasRows = 1;
        std::vector<std::string> m_frameNames;
    };
}
