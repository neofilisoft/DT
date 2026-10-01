// Copyright Neofilisoft. All Rights Reserved.
#include "core/animation/AnimationPose.h"

namespace lacrima::anim
{
    AnimationPose::AnimationPose(u32 boneCount)
    {
        Resize(boneCount);
    }

    void AnimationPose::Resize(u32 boneCount)
    {
        m_localTransforms.resize(boneCount);
        m_globalTransforms.resize(boneCount, Mat4::Identity());
        m_skinningPalette.resize(boneCount, Mat4::Identity());
    }

    void AnimationPose::SetLocalTransform(i32 boneIndex, const Vec3& pos, const Quat& rot, const Vec3& scale)
    {
        if (boneIndex >= 0 && boneIndex < static_cast<i32>(m_localTransforms.size()))
        {
            m_localTransforms[boneIndex] = BoneTransform{pos, rot, scale};
        }
    }

    void AnimationPose::SetLocalTransform(i32 boneIndex, const BoneTransform& transform)
    {
        if (boneIndex >= 0 && boneIndex < static_cast<i32>(m_localTransforms.size()))
        {
            m_localTransforms[boneIndex] = transform;
        }
    }

    const BoneTransform& AnimationPose::GetLocalTransform(i32 boneIndex) const
    {
        return m_localTransforms[boneIndex];
    }

    BoneTransform& AnimationPose::GetLocalTransform(i32 boneIndex)
    {
        return m_localTransforms[boneIndex];
    }

    const Mat4& AnimationPose::GetGlobalTransform(i32 boneIndex) const
    {
        return m_globalTransforms[boneIndex];
    }

    const Mat4& AnimationPose::GetSkinningMatrix(i32 boneIndex) const
    {
        return m_skinningPalette[boneIndex];
    }

    const f32* AnimationPose::GetSkinningPaletteData() const
    {
        return m_skinningPalette.empty() ? nullptr : &m_skinningPalette[0].m[0];
    }

    void AnimationPose::ComputeGlobalPose(const Skeleton& skeleton)
    {
        u32 count = static_cast<u32>(m_localTransforms.size());
        for (u32 i = 0; i < count; ++i)
        {
            const Bone* bone = skeleton.GetBone(static_cast<i32>(i));
            Mat4 localMat = m_localTransforms[i].ToMatrix();

            if (bone && bone->parentIndex >= 0 && bone->parentIndex < static_cast<i32>(i))
            {
                m_globalTransforms[i] = m_globalTransforms[bone->parentIndex] * localMat;
            }
            else
            {
                m_globalTransforms[i] = localMat;
            }
        }
    }

    void AnimationPose::ComputeSkinningPalette(const Skeleton& skeleton)
    {
        ComputeGlobalPose(skeleton);

        u32 count = static_cast<u32>(m_globalTransforms.size());
        for (u32 i = 0; i < count; ++i)
        {
            const Bone* bone = skeleton.GetBone(static_cast<i32>(i));
            if (bone)
            {
                m_skinningPalette[i] = m_globalTransforms[i] * bone->inverseBindPose;
            }
            else
            {
                m_skinningPalette[i] = m_globalTransforms[i];
            }
        }
    }

    void AnimationPose::ResetToBindPose(const Skeleton& skeleton)
    {
        u32 count = skeleton.GetBoneCount();
        Resize(count);

        for (u32 i = 0; i < count; ++i)
        {
            const Bone* bone = skeleton.GetBone(static_cast<i32>(i));
            if (bone)
            {
                m_localTransforms[i].translation = bone->localPosition;
                m_localTransforms[i].rotation = bone->localRotation;
                m_localTransforms[i].scale = bone->localScale;
                m_globalTransforms[i] = bone->globalBindPose;
            }
        }
        ComputeSkinningPalette(skeleton);
    }
}