#include "simulation/sprite/SpriteAnimationSystem.h"

#include <algorithm>

namespace lacrima::sim
{
    void SpriteAnimationSystem::Step(Sprite2DComponent& sprite, SpriteAnimationPlayer& player,
                                     const SpriteAnimationClip& clip, f32 deltaSeconds)
    {
        player.Play(clip, false);
        player.Advance(clip, deltaSeconds);

        if (static_cast<bool>(clip.atlasTexture))
            sprite.textureAsset = clip.atlasTexture;

        const u32 columns = std::max(1u, clip.atlasColumns);
        const u32 rows = std::max(1u, clip.atlasRows);
        const u32 atlasCapacity = columns * rows;
        const u32 atlasFrame = atlasCapacity > 0 ? player.frame % atlasCapacity : 0;
        const u32 column = atlasFrame % columns;
        const u32 row = atlasFrame / columns;

        sprite.uv.u = static_cast<f32>(column) / static_cast<f32>(columns);
        sprite.uv.v = static_cast<f32>(row) / static_cast<f32>(rows);
        sprite.uv.width = 1.0f / static_cast<f32>(columns);
        sprite.uv.height = 1.0f / static_cast<f32>(rows);
    }
}
