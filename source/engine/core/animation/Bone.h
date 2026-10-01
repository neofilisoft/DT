// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/math/Math.h"
#include <string>

namespace lacrima::anim
{
    static constexpr i32 kInvalidBoneIndex = -1;
    static constexpr u32 kMaxBones = 128;

    struct Bone
    {
        std::string name;
        i32 index = kInvalidBoneIndex;
        i32 parentIndex = kInvalidBoneIndex;

        // Local transform in bind pose relative to parent
        Vec3 localPosition{0.0f, 0.0f, 0.0f};
        Quat localRotation{0.0f, 0.0f, 0.0f, 1.0f};
        Vec3 localScale{1.0f, 1.0f, 1.0f};

        // Precalculated global transform in bind pose
        Mat4 globalBindPose = Mat4::Identity();

        // Inverse of globalBindPose: transforms vertex from model space to bone space
        Mat4 inverseBindPose = Mat4::Identity();
    };
}