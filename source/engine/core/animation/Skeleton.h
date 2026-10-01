// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/animation/Bone.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace lacrima::anim
{
    class Skeleton
    {
    public:
        Skeleton() = default;
        ~Skeleton() = default;

        // Adds a bone to the hierarchy. Bones should be added in root-to-leaf order.
        i32 AddBone(const std::string& name, i32 parentIndex, const Vec3& localPos, const Quat& localRot, const Vec3& localScale);

        // Sets explicit inverse bind pose matrix for a bone (e.g. loaded directly from GLTF/Assimp)
        void SetInverseBindPose(i32 boneIndex, const Mat4& invBindPose);

        // Reconstructs all global bind transforms and inverse bind matrices from local TRS hierarchically
        void BuildBindPoseHierarchy();

        i32 FindBoneIndex(const std::string& name) const;

        const Bone* GetBone(i32 index) const;
        Bone* GetBone(i32 index);

        u32 GetBoneCount() const { return static_cast<u32>(m_bones.size()); }
        const std::vector<Bone>& GetBones() const { return m_bones; }

    private:
        std::vector<Bone> m_bones;
        std::unordered_map<std::string, i32> m_nameToIndex;
    };
}