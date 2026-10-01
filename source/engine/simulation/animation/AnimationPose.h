// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// AnimationPose.h - Lacrima Engine Skeletal Animation (Step 3)
//
// Evaluates a full skeleton pose (global bone transforms) from
// per-bone local TRS data, then computes the GPU skin matrix palette:
//
//   SkinMatrix_i = GlobalBoneTransform_i * inverseBindPose_i
//
// The resulting array of Mat4 (one per bone, max 128) is uploaded to the
// GPU storage buffer (BonePalette) consumed by skinned_mesh.vert.
// ---------------------------------------------------------------------------

#include "simulation/animation/Skeleton.h"
#include "simulation/animation/AnimationClip.h"
#include <vector>

namespace lacrima::sim
{
    class AnimationPose
    {
    public:
        // Capacity constants
        static constexpr usize kMaxBones = 128;

        // Initialize storage for a given skeleton bone count
        void Initialize(usize boneCount);

        // Reset local transforms to skeleton bind pose
        void ResetToBindPose(const Skeleton& skeleton);

        // Write sampled local TRS from an animation clip at time t
        void ApplyClipSample(
            f32 t,
            const AnimationClip& clip,
            const Skeleton& skeleton);

        // Blend two poses by weight (0 = poseA, 1 = poseB)
        void BlendWith(const AnimationPose& other, f32 alpha);

        // Compute global bone transforms and GPU skin matrix palette.
        // outSkinMatrices must have at least BoneCount() entries.
        void ComputeSkinMatrices(
            const Skeleton& skeleton,
            std::vector<Mat4>& outSkinMatrices) const;

        usize BoneCount() const { return m_localPositions.size(); }

    private:
        // Per-bone local transforms relative to parent
        std::vector<Vec3> m_localPositions;
        std::vector<Quat> m_localRotations;
        std::vector<Vec3> m_localScales;
    };
}
