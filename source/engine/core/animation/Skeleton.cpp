// Copyright Neofilisoft. All Rights Reserved.
#include "core/animation/Skeleton.h"

namespace lacrima::anim
{
    i32 Skeleton::AddBone(const std::string& name, i32 parentIndex, const Vec3& localPos, const Quat& localRot, const Vec3& localScale)
    {
        i32 newIndex = static_cast<i32>(m_bones.size());
        Bone bone;
        bone.name = name;
        bone.index = newIndex;
        bone.parentIndex = parentIndex;
        bone.localPosition = localPos;
        bone.localRotation = localRot;
        bone.localScale = localScale;

        Mat4 localTransform = Mat4::TRS(localPos, localRot, localScale);
        if (parentIndex >= 0 && parentIndex < static_cast<i32>(m_bones.size()))
        {
            bone.globalBindPose = m_bones[parentIndex].globalBindPose * localTransform;
        }
        else
        {
            bone.globalBindPose = localTransform;
        }
        bone.inverseBindPose = bone.globalBindPose.Inverted();

        m_bones.push_back(bone);
        m_nameToIndex[name] = newIndex;
        return newIndex;
    }

    void Skeleton::SetInverseBindPose(i32 boneIndex, const Mat4& invBindPose)
    {
        if (boneIndex >= 0 && boneIndex < static_cast<i32>(m_bones.size()))
        {
            m_bones[boneIndex].inverseBindPose = invBindPose;
        }
    }

    void Skeleton::BuildBindPoseHierarchy()
    {
        for (size_t i = 0; i < m_bones.size(); ++i)
        {
            Bone& bone = m_bones[i];
            Mat4 localTransform = Mat4::TRS(bone.localPosition, bone.localRotation, bone.localScale);
            if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<i32>(i))
            {
                bone.globalBindPose = m_bones[bone.parentIndex].globalBindPose * localTransform;
            }
            else
            {
                bone.globalBindPose = localTransform;
            }
            bone.inverseBindPose = bone.globalBindPose.Inverted();
        }
    }

    i32 Skeleton::FindBoneIndex(const std::string& name) const
    {
        auto it = m_nameToIndex.find(name);
        if (it != m_nameToIndex.end())
        {
            return it->second;
        }
        return kInvalidBoneIndex;
    }

    const Bone* Skeleton::GetBone(i32 index) const
    {
        if (index >= 0 && index < static_cast<i32>(m_bones.size()))
        {
            return &m_bones[index];
        }
        return nullptr;
    }

    Bone* Skeleton::GetBone(i32 index)
    {
        if (index >= 0 && index < static_cast<i32>(m_bones.size()))
        {
            return &m_bones[index];
        }
        return nullptr;
    }
}