// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"

namespace lacrima::action
{
    enum class FrameState : u8
    {
        Idle,
        Startup,
        Active,
        Recovery,
        Hitstun,
        Blockstun,
        Knockdown
    };

    struct FrameData
    {
        FrameState currentState = FrameState::Idle;
        u32 currentFrame = 0;
        u32 maxFrames = 0;
        
        void TransitionTo(FrameState newState, u32 durationFrames = 0)
        {
            currentState = newState;
            currentFrame = 0;
            maxFrames = durationFrames;
        }

        bool IsFinished() const
        {
            return maxFrames > 0 && currentFrame >= maxFrames;
        }
    };
}
