// Copyright Neofilisoft. All Rights Reserved.
#include "simulation/animation/SkeletalAnimationSystem.h"

namespace lacrima::sim
{
    void SkeletalAnimationSystem::Update(
        f32 fixedDeltaSeconds,
        ComponentArray<Entity, SkeletalMeshComponent>& skeletalMeshes)
    {
        skeletalMeshes.ForEach([fixedDeltaSeconds](Entity, SkeletalMeshComponent& smc)
        {
            if (!smc.skeleton || !smc.isPlaying) return;

            const f32 dt = fixedDeltaSeconds * smc.playbackSpeed;

            // ---------------------------------------------------------------------------
            // 1. Advance playback clocks
            // ---------------------------------------------------------------------------
            if (smc.usesCrossFade)
            {
                smc.crossFadeFromTime += dt;
                smc.crossFadeToTime   += dt;
                const bool stillBlending = smc.crossFader.Tick(fixedDeltaSeconds);
                if (!stillBlending)
                {
                    // Blend complete - promote to-time as main time, disable crossfade
                    smc.animationTime = smc.crossFadeToTime;
                    smc.usesCrossFade = false;
                }
            }
            else
            {
                smc.animationTime += dt;
            }

            // ---------------------------------------------------------------------------
            // 2. Evaluate pose
            // ---------------------------------------------------------------------------
            if (smc.usesCrossFade)
            {
                smc.crossFader.Evaluate(
                    smc.crossFadeFromTime,
                    smc.crossFadeToTime,
                    *smc.skeleton,
                    smc.currentPose);
            }
            else if (smc.useBlendSpace)
            {
                smc.blendSpace.Evaluate(
                    smc.blendParam,
                    smc.animationTime,
                    *smc.skeleton,
                    smc.currentPose);
            }
            else if (smc.activeClipIndex >= 0 &&
                     smc.activeClipIndex < static_cast<i32>(smc.clips.size()))
            {
                smc.currentPose.ApplyClipSample(
                    smc.animationTime,
                    smc.clips[smc.activeClipIndex],
                    *smc.skeleton);
            }
            else
            {
                smc.currentPose.ResetToBindPose(*smc.skeleton);
            }

            // ---------------------------------------------------------------------------
            // 3. Compute GPU skin matrix palette
            // ---------------------------------------------------------------------------
            smc.currentPose.ComputeSkinMatrices(*smc.skeleton, smc.boneMatrices);
        });
    }
}
