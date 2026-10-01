// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/math/Math.h"
#include "core/animation/Skeleton.h"
#include <vector>

namespace lacrima::anim
{
    struct BoneTransform
    {
        Vec3 translation{0.0f, 0.0f, 0.0f};
        Quat rotation{0.0f, 0.0f, 0.0f, 1.0f};
        Vec3 scale{1.0f, 1.0f, 1.0f};

        Mat4 ToMatrix() const
        {
            return Mat4::TRS(translation, rotation, scale);
        }
    };

    class AnimationPose
    {
    public:
        AnimationPose() = default;
        explicit AnimationPose(u32 boneCount);

        void Resize(u32 boneCount);
        u32 GetBoneCount() const { return static_cast<u32>(m_localTransforms.size()); }

        void SetLocalTransform(i32 boneIndex, const Vec3& pos, const Quat& rot, const Vec3& scale);
        void SetLocalTransform(i32 boneIndex, const BoneTransform& transform);

        const BoneTransform& GetLocalTransform(i32 boneIndex) const;
        BoneTransform& GetLocalTransform(i32 boneIndex);

        const Mat4& GetGlobalTransform(i32 boneIndex) const;
        const Mat4& GetSkinningMatrix(i32 boneIndex) const;

        const std::vector<Mat4>& GetSkinningPalette() const { return m_skinningPalette; }
        const f32* GetSkinningPaletteData() const;

        // Computes global matrices from local transforms using skeleton parent hierarchy
        void ComputeGlobalPose(const Skeleton& skeleton);

        // Computes final GPU skinning matrices: skinningMatrix = globalMatrix * inverseBindPose
        void ComputeSkinningPalette(const Skeleton& skeleton);

        // Resets pose to the skeleton's reference bind pose
        void ResetToBindPose(const Skeleton& skeleton);

    private:
        std::vector<BoneTransform> m_localTransforms;
        std::vector<Mat4> m_globalTransforms;
        std::vector<Mat4> m_skinningPalette;
    };
}