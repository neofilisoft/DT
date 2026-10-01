// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/animation/AnimationClip.h"
#include "core/animation/AnimationPose.h"
#include <vector>

namespace lacrima::anim
{
    class AnimBlender
    {
    public:
        // Evaluates a single animation clip at specified time into an animation pose
        static void SampleClip(const AnimationClip& clip, f32 time, AnimationPose& outPose);

        // Blends two poses linearly (position/scale with Lerp, rotation with Slerp)
        // weight = 0.0 -> 100% poseA, weight = 1.0 -> 100% poseB
        static void BlendPoses(const AnimationPose& poseA, const AnimationPose& poseB, f32 weight, AnimationPose& outPose);
    };

    struct BlendPoint1D
    {
        f32 value = 0.0f;
        const AnimationClip* clip = nullptr;
    };

    // 1D Blend Space for smooth speed/direction transitions (e.g. Idle -> Walk -> Run)
    class BlendSpace1D
    {
    public:
        BlendSpace1D() = default;

        void AddSample(f32 parameterValue, const AnimationClip* clip);

        // Evaluates blend space at given parameter value and time
        void Evaluate(f32 parameterValue, f32 time, AnimationPose& outPose) const;

    private:
        std::vector<BlendPoint1D> m_samples;
    };

    // Smooth state machine transition cross-fader
    class CrossFader
    {
    public:
        CrossFader() = default;

        void StartTransition(f32 transitionDurationSeconds);
        void Update(f32 deltaTime);

        bool IsTransitioning() const { return m_isTransitioning; }
        f32 GetBlendWeight() const { return m_blendWeight; }

    private:
        f32 m_duration = 0.2f;
        f32 m_elapsed = 0.0f;
        f32 m_blendWeight = 1.0f;
        bool m_isTransitioning = false;
    };
}