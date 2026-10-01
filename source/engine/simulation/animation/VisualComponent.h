// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"
#include "core/string/StringID.h"
#include <string>

namespace lacrima::sim
{
    // Simple state machine states for M10.
    // In a future data-driven version, this might just be hashed strings.
    enum class AnimationState : u8
    {
        Idle,
        Walk,
        Interact,
        Count
    };

    inline const char* ToString(AnimationState state)
    {
        switch (state)
        {
            case AnimationState::Idle: return "Idle";
            case AnimationState::Walk: return "Walk";
            case AnimationState::Interact: return "Interact";
            default: return "Unknown";
        }
    }

    struct VisualComponent
    {
        lacrima::StringID visualId;
        
        AnimationState currentState = AnimationState::Idle;
        
        // Playback state
        f32 currentFrame = 0.0f;
        f32 playbackSpeed = 1.0f; // frames per second (sim time)
        bool isLooping = true;
        
        // Visual config (would normally come from an Asset)
        u32 totalFrames = 1;
        u32 framesPerRow = 1;

        lacrima::StringID assetPath;
        float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

        REFLECT_BEGIN(VisualComponent)
            REFLECT_FIELD(visualId)
            REFLECT_FIELD(assetPath)
            REFLECT_FIELD(currentState)
            REFLECT_FIELD(currentFrame)
            REFLECT_FIELD(playbackSpeed)
            REFLECT_FIELD(isLooping)
            REFLECT_FIELD(totalFrames)
            REFLECT_FIELD(framesPerRow)
        REFLECT_END()
    };
}

