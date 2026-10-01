// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/string/StringID.h"
#include "core/reflection/Reflection.h"

#include <algorithm>
#include <cmath>

namespace lacrima::sim
{
    // A clip is data, not behavior. It can later be populated by a cooked
    // .asset file or generated from a directory of numbered PNG frames.
    struct SpriteAnimationClip
    {
        StringID id;
        StringID atlasTexture;
        u32 firstFrame = 0;
        u32 frameCount = 1;
        u32 atlasColumns = 1;
        u32 atlasRows = 1;
        f32 framesPerSecond = 1.0f;
        bool looping = true;

        REFLECT_BEGIN(SpriteAnimationClip)
            REFLECT_FIELD(id)
            REFLECT_FIELD(atlasTexture)
            REFLECT_FIELD(firstFrame)
            REFLECT_FIELD(frameCount)
            REFLECT_FIELD(atlasColumns)
            REFLECT_FIELD(atlasRows)
            REFLECT_FIELD(framesPerSecond)
            REFLECT_FIELD(looping)
        REFLECT_END()
    };

    // Deterministic playback state. It uses simulation time, so animation
    // results are independent of render FPS and suitable for save/replay.
    struct SpriteAnimationPlayer
    {
        StringID clip;
        f32 timeSeconds = 0.0f;
        u32 frame = 0;
        bool finished = false;

        REFLECT_BEGIN(SpriteAnimationPlayer)
            REFLECT_FIELD(clip)
            REFLECT_FIELD(timeSeconds)
            REFLECT_FIELD(frame)
            REFLECT_FIELD(finished)
        REFLECT_END()

        void Play(const SpriteAnimationClip& definition, bool restart = true)
        {
            if (restart || clip != definition.id)
            {
                timeSeconds = 0.0f;
                frame = definition.firstFrame;
                finished = false;
            }
            clip = definition.id;
        }

        void Advance(const SpriteAnimationClip& definition, f32 deltaSeconds)
        {
            if (definition.frameCount == 0 || definition.framesPerSecond <= 0.0f || finished)
                return;

            timeSeconds = std::max(0.0f, timeSeconds + deltaSeconds);
            const f32 localFrame = std::floor(timeSeconds * definition.framesPerSecond);

            if (definition.looping)
            {
                const u32 offset = static_cast<u32>(localFrame) % definition.frameCount;
                frame = definition.firstFrame + offset;
            }
            else
            {
                const u32 offset = static_cast<u32>(localFrame);
                if (offset >= definition.frameCount - 1)
                {
                    frame = definition.firstFrame + definition.frameCount - 1;
                    finished = true;
                }
                else
                {
                    frame = definition.firstFrame + offset;
                }
            }
        }
    };
}

