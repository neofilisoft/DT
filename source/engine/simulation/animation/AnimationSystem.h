#pragma once

#include "core/platform/Types.h"
#include "core/containers/ComponentArray.h"
#include "runtime/Entity.h"
#include "simulation/animation/VisualComponent.h"

namespace dt::sim
{
    class AnimationSystem
    {
    public:
        // Steps the animation frames forward based on delta time and playback speed
        static void StepAnimations(
            f32 fixedDeltaSeconds,
            ComponentArray<Entity, VisualComponent>& visuals);
            
        // Setup visual config (hardcoded for now, would be asset-driven later)
        static void SetupSpriteAnimation(VisualComponent& visual, AnimationState state);
    };
}
