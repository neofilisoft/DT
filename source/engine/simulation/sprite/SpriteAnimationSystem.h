#pragma once

#include "simulation/sprite/Sprite2DComponent.h"
#include "simulation/sprite/SpriteAnimation.h"

namespace lacrima::sim
{
    // Applies deterministic clip playback to a Sprite2DComponent. Atlas
    // coordinates are derived from the clip's row/column metadata, keeping
    // game code independent of renderer-specific UV math.
    class SpriteAnimationSystem
    {
    public:
        static void Step(Sprite2DComponent& sprite, SpriteAnimationPlayer& player,
                         const SpriteAnimationClip& clip, f32 deltaSeconds);
    };
}
