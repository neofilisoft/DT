// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// SkeletalMeshComponent.h - Lacrima Engine ECS Component (Step 3)
//
// Attached to a 3D entity to drive skeletal animation and GPU skinning.
// Owns:
//   - A reference to a shared Skeleton asset
//   - One or more AnimationClips (loaded from GLTF/FBX assets)
//   - An AnimationPose (current evaluated pose)
//   - A CrossFader for smooth clip transitions
//   - A BlendSpace1D for speed-based locomotion blending (Idle/Walk/Run)
//   - The computed bone skin matrix palette (uploaded each frame to GPU)
//
// SkeletalAnimationSystem ticks this component each frame.
// SkinnedMeshPass reads boneMatrices[] and uploads to the GPU storage buffer.
// ---------------------------------------------------------------------------

#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"
#include "simulation/animation/Skeleton.h"
#include "simulation/animation/AnimationClip.h"
#include "simulation/animation/AnimationPose.h"
#include "simulation/animation/AnimBlender.h"
#include <vector>
#include <memory>
#include <string>

namespace lacrima::sim
{
    struct SkeletalMeshComponent
    {
        // ---------------------------------------------------------------------------
        // Asset references (set by asset pipeline / editor)
        // ---------------------------------------------------------------------------
        std::shared_ptr<Skeleton>       skeleton;     // Shared skeleton hierarchy
        std::vector<AnimationClip>      clips;        // All loaded animation clips

        // Currently active clip index (-1 = none / T-pose)
        i32 activeClipIndex = -1;

        // Playback state
        f32 animationTime   = 0.0f;  // Current clip playback time (seconds)
        f32 playbackSpeed   = 1.0f;  // Multiplier for playback speed
        bool isPlaying      = true;

        // ---------------------------------------------------------------------------
        // 1D Blend space (locomotion)
        // ---------------------------------------------------------------------------
        BlendSpace1D blendSpace;
        f32          blendParam      = 0.0f;   // e.g. normalized speed [0..1]
        bool         useBlendSpace   = false;  // When true, drives pose via blendSpace

        // ---------------------------------------------------------------------------
        // Cross-fade / state transition
        // ---------------------------------------------------------------------------
        CrossFader crossFader;
        f32        crossFadeFromTime = 0.0f;
        f32        crossFadeToTime   = 0.0f;
        bool       usesCrossFade     = false;

        // ---------------------------------------------------------------------------
        // Computed output (written by SkeletalAnimationSystem, read by SkinnedMeshPass)
        // ---------------------------------------------------------------------------
        AnimationPose currentPose;

        // Final skin matrix palette - indexed by bone index.
        // Uploaded to GPU storage buffer each frame.
        std::vector<Mat4> boneMatrices;

        // ---------------------------------------------------------------------------
        // Helpers
        // ---------------------------------------------------------------------------

        // Initialize for a given skeleton (must be called after skeleton is set)
        void Initialize()
        {
            if (!skeleton) return;
            currentPose.Initialize(skeleton->BoneCount());
            currentPose.ResetToBindPose(*skeleton);
            boneMatrices.resize(skeleton->BoneCount(), Mat4::Identity());
        }

        // Trigger a smooth transition to a clip by name (CrossFade)
        void PlayClip(const std::string& clipName, f32 blendDuration = 0.2f)
        {
            if (!skeleton) return;
            for (i32 i = 0; i < static_cast<i32>(clips.size()); ++i)
            {
                if (clips[i].name == clipName)
                {
                    const AnimationClip* fromClip = (activeClipIndex >= 0) ? &clips[activeClipIndex] : nullptr;
                    crossFader.StartTransition(fromClip, &clips[i], blendDuration);
                    crossFadeFromTime = animationTime;
                    crossFadeToTime   = 0.0f;
                    usesCrossFade     = true;
                    activeClipIndex   = i;
                    return;
                }
            }
        }

        REFLECT_BEGIN(SkeletalMeshComponent)
            REFLECT_FIELD(activeClipIndex)
            REFLECT_FIELD(animationTime)
            REFLECT_FIELD(playbackSpeed)
            REFLECT_FIELD(isPlaying)
            REFLECT_FIELD(blendParam)
            REFLECT_FIELD(useBlendSpace)
        REFLECT_END()
    };
}
