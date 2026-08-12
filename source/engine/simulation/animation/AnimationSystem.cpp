#include "simulation/animation/AnimationSystem.h"
#include "runtime/Entity.h"

namespace dt::sim
{
    void AnimationSystem::StepAnimations(f32 fixedDeltaSeconds, ComponentArray<Entity, VisualComponent>& visuals)
    {
        visuals.ForEach([fixedDeltaSeconds](Entity, VisualComponent& visual)
        {
            if (visual.totalFrames <= 1)
                return;

            visual.currentFrame += visual.playbackSpeed * fixedDeltaSeconds;

            if (visual.isLooping)
            {
                while (visual.currentFrame >= visual.totalFrames)
                {
                    visual.currentFrame -= visual.totalFrames;
                }
            }
            else
            {
                if (visual.currentFrame >= visual.totalFrames)
                {
                    visual.currentFrame = static_cast<f32>(visual.totalFrames - 1);
                }
            }
        });
    }

    void AnimationSystem::SetupSpriteAnimation(VisualComponent& visual, AnimationState state)
    {
        visual.currentState = state;
        visual.currentFrame = 0.0f;

        // Hardcoded mapping for M10 testing.
        // Assuming a simple spritesheet for testing:
        // Idle: row 0, 4 frames
        // Walk: row 1, 6 frames
        // Interact: row 2, 4 frames
        switch (state)
        {
            case AnimationState::Idle:
                visual.totalFrames = 4;
                visual.framesPerRow = 4;
                visual.playbackSpeed = 4.0f;
                visual.isLooping = true;
                break;
            case AnimationState::Walk:
                visual.totalFrames = 6;
                visual.framesPerRow = 6;
                visual.playbackSpeed = 8.0f;
                visual.isLooping = true;
                break;
            case AnimationState::Interact:
                visual.totalFrames = 4;
                visual.framesPerRow = 4;
                visual.playbackSpeed = 6.0f;
                visual.isLooping = false;
                break;
            default:
                visual.totalFrames = 1;
                visual.framesPerRow = 1;
                visual.playbackSpeed = 0.0f;
                break;
        }
    }
}
