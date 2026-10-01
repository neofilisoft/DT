// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// Skeleton.h - Lacrima Engine Skeletal Animation (Step 3)
//
// Represents the full bone hierarchy of a character.
// Contains ordered array of Bones (parent index always < child index),
// name lookup map, and the pre-baked inverse bind pose matrices.
//
// Usage:
//   1. Load bones from an asset (tinygltf/assimp populates via AddBone).
//   2. Call BakeInverseBindPoses() once to pre-compute all inverseBindPose.
//   3. Pass Skeleton reference to SkeletalMeshComponent and animation clips.
// ---------------------------------------------------------------------------

#include "simulation/animation/Bone.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace lacrima::sim
{
    class Skeleton
    {
    public:
        // Add a bone. parentIndex must be < boneIndex (topological order enforced).
        // Returns the index assigned to this bone.
        i32 AddBone(Bone bone)
        {
            const i32 index = static_cast<i32>(m_bones.size());
            m_nameToIndex[bone.name] = index;
            m_bones.push_back(std::move(bone));
            return index;
        }

        // Bake global inverse bind pose matrices for all bones.
        // Must be called after all bones are added.
        void BakeInverseBindPoses();

        i32 FindBone(const std::string& name) const
        {
            auto it = m_nameToIndex.find(name);
            return (it != m_nameToIndex.end()) ? it->second : -1;
        }

        usize BoneCount() const { return m_bones.size(); }
        const Bone& GetBone(i32 index) const { return m_bones[static_cast<usize>(index)]; }
        Bone& GetBone(i32 index) { return m_bones[static_cast<usize>(index)]; }

        const std::vector<Bone>& Bones() const { return m_bones; }

    private:
        std::vector<Bone> m_bones;
        std::unordered_map<std::string, i32> m_nameToIndex;
    };
}
