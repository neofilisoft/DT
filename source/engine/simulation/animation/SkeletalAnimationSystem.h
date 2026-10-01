// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// SkeletalAnimationSystem.h - Lacrima Engine ECS System (Step 3)
//
// Ticks all SkeletalMeshComponent instances each frame:
//   1. Advances animationTime by deltaTime * playbackSpeed
//   2. Evaluates the current pose via BlendSpace1D or CrossFader or direct clip
//   3. Calls AnimationPose::ComputeSkinMatrices() to produce GPU bone palette
//
// The resulting boneMatrices[] on each component is read by SkinnedMeshPass
// and uploaded to the GPU storage buffer (BonePalette) for skinned_mesh.vert.
// ---------------------------------------------------------------------------

#include "core/platform/Types.h"
#include "core/containers/ComponentArray.h"
#include "runtime/Entity.h"
#include "simulation/animation/SkeletalMeshComponent.h"

namespace lacrima::sim
{
    class SkeletalAnimationSystem
    {
    public:
        // Advance all skeletal mesh animations and compute skin matrix palettes.
        // Call once per tick before SkinnedMeshPass::SetupFrame.
        static void Update(
            f32 fixedDeltaSeconds,
            ComponentArray<Entity, SkeletalMeshComponent>& skeletalMeshes);
    };
}
