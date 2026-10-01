// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/string/StringID.h"
#include "core/reflection/Reflection.h"
#include "simulation/sprite/SpriteAnimation.h"

namespace lacrima::sim
{
    // Normalized texture rectangle. Keeping the rectangle in UV space lets
    // the same component address a standalone PNG, an atlas entry, or a
    // cooked texture without changing gameplay code.
    struct SpriteUVRect
    {
        f32 u = 0.0f;
        f32 v = 0.0f;
        f32 width = 1.0f;
        f32 height = 1.0f;
    };

    // Runtime-facing 2D sprite state. This is deliberately independent of
    // Vulkan and of any particular asset format; the renderer receives a
    // flat projection of it through SimSnapshot.
    struct Sprite2DComponent
    {
        StringID textureAsset;
        SpriteUVRect uv;

        // World-space size and normalized pivot. A centered pivot is the
        // Unity Sprite.Create default used by the original game.
        f32 width = 1.0f;
        f32 height = 1.0f;
        f32 pivotX = 0.5f;
        f32 pivotY = 0.5f;

        f32 tintR = 1.0f;
        f32 tintG = 1.0f;
        f32 tintB = 1.0f;
        f32 tintA = 1.0f;

        i32 renderLayer = 0;
        bool billboard = true;
        bool pixelPerfect = true;
        bool flipX = false;
        bool flipY = false;

        // Optional deterministic atlas animation owned by this sprite.
        bool animationEnabled = false;
        SpriteAnimationClip animationClip;
        SpriteAnimationPlayer animationPlayer;

        REFLECT_BEGIN(Sprite2DComponent)
            REFLECT_FIELD(textureAsset)
            REFLECT_FIELD(width)
            REFLECT_FIELD(height)
            REFLECT_FIELD(pivotX)
            REFLECT_FIELD(pivotY)
            REFLECT_FIELD(tintR)
            REFLECT_FIELD(tintG)
            REFLECT_FIELD(tintB)
            REFLECT_FIELD(tintA)
            REFLECT_FIELD(renderLayer)
            REFLECT_FIELD(animationEnabled)
            REFLECT_FIELD(animationClip)
            REFLECT_FIELD(animationPlayer)
        REFLECT_END()
    };
}

