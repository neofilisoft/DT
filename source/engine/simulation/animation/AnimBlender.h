// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// AnimBlender.h - Lacrima Engine Skeletal Animation (Step 3)
//
// Provides two blending modes:
//
// 1. BlendSpace1D - Linear 1D blend tree (Idle -> Walk -> Run by speed).
//    Maps a scalar parameter (e.g. speed 0..1) to a weighted blend of
//    up to N AnimationClip entries at fixed parameter positions.
//
// 2. CrossFader - Smooth transition between a "from" and "to" clip
//    over a specified blend duration. Useful for state-machine transitions.
//    After duration expires the crossfade is complete and only the target
//    clip plays.
//
// Both operate on AnimationPose to produce the final blended pose.
// ---------------------------------------------------------------------------

#include "simulation/animation/AnimationPose.h"
#include "simulation/animation/Skeleton.h"
#include "simulation/animation/AnimationClip.h"
#include <vector>

namespace lacrima::sim
{
    // -----------------------------------------------------------------------
    // BlendSpace1D
    // A 1D blend space maps a float parameter (e.g. character speed) to a
    // weighted mix of N animation clips placed at explicit parameter values.
    // -----------------------------------------------------------------------
    class BlendSpace1D
    {
    public:
        struct BlendPoint
        {
            f32                  paramValue; // e.g. 0.0 = Idle, 0.5 = Walk, 1.0 = Run
            const AnimationClip* clip = nullptr;
        };

        // Add a clip at a parameter position. Must add in ascending paramValue order.
        void AddPoint(f32 paramValue, const AnimationClip* clip)
        {
            m_points.push_back({paramValue, clip});
        }

        // Evaluate blended pose for parameter t.
        // time is the animation playback time (advanced externally per tick).
        void Evaluate(
            f32 param,
            f32 time,
            const Skeleton& skeleton,
            AnimationPose& outPose) const;

    private:
        std::vector<BlendPoint> m_points;
    };

    // -----------------------------------------------------------------------
    // CrossFader
    // Smoothly transitions from clipFrom to clipTo over blendDuration seconds.
    // -----------------------------------------------------------------------
    class CrossFader
    {
    public:
        // Start a crossfade from the current clip to a new one.
        // blendDuration: how many seconds the blend takes.
        void StartTransition(const AnimationClip* from, const AnimationClip* to, f32 blendDuration);

        // Advance the blend timer. Call once per tick with deltaTime.
        // Returns true if the fade is still in progress (false when complete).
        bool Tick(f32 deltaTime);

        // Evaluate the blended pose at the current blend alpha.
        // fromTime and toTime are the playback clocks managed externally.
        void Evaluate(
            f32 fromTime,
            f32 toTime,
            const Skeleton& skeleton,
            AnimationPose& outPose) const;

        bool IsTransitioning() const { return m_blendElapsed < m_blendDuration; }
        f32  BlendAlpha()      const;
        const AnimationClip* CurrentClip() const { return m_clipTo; }

    private:
        const AnimationClip* m_clipFrom    = nullptr;
        const AnimationClip* m_clipTo      = nullptr;
        f32                  m_blendDuration = 0.2f;
        f32                  m_blendElapsed  = 1.0f; // starts complete (no fade active)
    };
}
