// Copyright Neofilisoft. All Rights Reserved.
#include "simulation/animation/AnimationPose.h"
#include <algorithm>
#include <cmath>

namespace lacrima::sim
{
    void AnimationPose::Initialize(usize boneCount)
    {
        const usize clamped = std::min(boneCount, kMaxBones);
        m_localPositions.assign(clamped, Vec3(0.0f, 0.0f, 0.0f));
        m_localRotations.assign(clamped, Quat(0.0f, 0.0f, 0.0f, 1.0f));
        m_localScales.assign(clamped, Vec3(1.0f, 1.0f, 1.0f));
    }

    void AnimationPose::ResetToBindPose(const Skeleton& skeleton)
    {
        const usize count = std::min(skeleton.BoneCount(), kMaxBones);
        m_localPositions.resize(count);
        m_localRotations.resize(count);
        m_localScales.resize(count);

        for (usize i = 0; i < count; ++i)
        {
            const Bone& bone = skeleton.GetBone(static_cast<i32>(i));
            m_localPositions[i] = bone.localBindPosition;
            m_localRotations[i] = bone.localBindRotation;
            m_localScales[i]    = bone.localBindScale;
        }
    }

    void AnimationPose::ApplyClipSample(
        f32 t,
        const AnimationClip& clip,
        const Skeleton& skeleton)
    {
        // Start from bind pose to fill bones with no track
        ResetToBindPose(skeleton);

        // Overwrite with sampled values from clip tracks
        clip.Sample(t, m_localPositions, m_localRotations, m_localScales);
    }

    void AnimationPose::BlendWith(const AnimationPose& other, f32 alpha)
    {
        const usize count = std::min(m_localPositions.size(), other.m_localPositions.size());
        for (usize i = 0; i < count; ++i)
        {
            m_localPositions[i] = Vec3::Lerp(m_localPositions[i], other.m_localPositions[i], alpha);
            m_localRotations[i] = Quat::Slerp(m_localRotations[i], other.m_localRotations[i], alpha);
            m_localScales[i]    = Vec3::Lerp(m_localScales[i], other.m_localScales[i], alpha);
        }
    }

    void AnimationPose::ComputeSkinMatrices(
        const Skeleton& skeleton,
        std::vector<Mat4>& outSkinMatrices) const
    {
        const usize boneCount = std::min(skeleton.BoneCount(), kMaxBones);
        outSkinMatrices.resize(boneCount, Mat4::Identity());

        // Compute global bone transforms in topological order (parent < child)
        std::vector<Mat4> globalTransforms(boneCount, Mat4::Identity());

        for (usize i = 0; i < boneCount; ++i)
        {
            // Build local TRS matrix
            Mat4 localRotMat = Mat4::FromQuat(m_localRotations[i]);
            Mat4 localT = Mat4::Translation(m_localPositions[i]);
            // Apply scale to rotation columns
            localRotMat.m[0]  *= m_localScales[i].x; localRotMat.m[1]  *= m_localScales[i].x; localRotMat.m[2]  *= m_localScales[i].x;
            localRotMat.m[4]  *= m_localScales[i].y; localRotMat.m[5]  *= m_localScales[i].y; localRotMat.m[6]  *= m_localScales[i].y;
            localRotMat.m[8]  *= m_localScales[i].z; localRotMat.m[9]  *= m_localScales[i].z; localRotMat.m[10] *= m_localScales[i].z;
            Mat4 localTRS = localT * localRotMat;

            const Bone& bone = skeleton.GetBone(static_cast<i32>(i));
            if (bone.parentIndex < 0)
            {
                globalTransforms[i] = localTRS;
            }
            else
            {
                globalTransforms[i] = globalTransforms[static_cast<usize>(bone.parentIndex)] * localTRS;
            }

            // SkinMatrix = GlobalBoneTransform * InverseBindPose
            outSkinMatrices[i] = globalTransforms[i] * bone.inverseBindPose;
        }
    }
}
