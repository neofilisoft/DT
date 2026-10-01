// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// Bone.h - Lacrima Engine Skeletal Animation (Step 3)
//
// Represents a single joint in a skeleton hierarchy.
// Stores the bone name, parent index, local bind pose transform, and
// the pre-computed inverse bind pose matrix (used for GPU skinning).
//
// The inverse bind pose converts a vertex from local mesh space into
// bone-local space so that bone transformations can be applied correctly:
//
//   SkinMatrix_i = GlobalBoneTransform_i * InverseBindPose_i
//
// A vertex deformed by bone i uses:
//   SkinnedPos = sum(weight_i * SkinMatrix_i * vertex_position)
// ---------------------------------------------------------------------------

#include "core/platform/Types.h"
#include "core/math/Math.h"
#include <string>

namespace lacrima::sim
{
    struct Bone
    {
        std::string name;

        // Index into the parent Skeleton's bones array.
        // -1 means this is a root bone (no parent).
        i32 parentIndex = -1;

        // Local transform relative to parent bone in the bind (rest) pose.
        Vec3 localBindPosition{0.0f, 0.0f, 0.0f};
        Quat localBindRotation{0.0f, 0.0f, 0.0f, 1.0f};
        Vec3 localBindScale{1.0f, 1.0f, 1.0f};

        // Inverse of the global (world-space) bind pose transform.
        // Pre-computed by Skeleton::BakeInverseBindPoses().
        // SkinMatrix = GlobalBoneTransform * inverseBindPose
        Mat4 inverseBindPose = Mat4::Identity();
    };
}
