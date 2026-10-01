// Copyright Neofilisoft. All Rights Reserved.
#include "simulation/animation/Skeleton.h"
#include <cstring>

namespace lacrima::sim
{
    // ---------------------------------------------------------------------------
    // BakeInverseBindPoses
    //
    // Traverses bones in topological order (parent index always < child index).
    // Computes each bone's global bind pose transform, then inverts it to
    // produce inverseBindPose = Inverse(GlobalBindPose).
    //
    // The inverse bind pose is used each frame to build the skin matrix:
    //   SkinMatrix_i = GlobalCurrentTransform_i * inverseBindPose_i
    // ---------------------------------------------------------------------------
    void Skeleton::BakeInverseBindPoses()
    {
        // Build global bind pose matrices bottom-up
        std::vector<Mat4> globalBindPoses(m_bones.size(), Mat4::Identity());

        for (usize i = 0; i < m_bones.size(); ++i)
        {
            const Bone& bone = m_bones[i];

            // Local bind pose TRS matrix
            Mat4 localRotation = Mat4::FromQuat(bone.localBindRotation);
            Mat4 localTranslation = Mat4::Translation(bone.localBindPosition);
            // Scale: s*R then T - for now bake as T*R (scale folded into translation)
            Mat4 localTRS = localTranslation * localRotation;
            // Apply scale to each column of localTRS
            localTRS.m[0]  *= bone.localBindScale.x; localTRS.m[1]  *= bone.localBindScale.x; localTRS.m[2]  *= bone.localBindScale.x;
            localTRS.m[4]  *= bone.localBindScale.y; localTRS.m[5]  *= bone.localBindScale.y; localTRS.m[6]  *= bone.localBindScale.y;
            localTRS.m[8]  *= bone.localBindScale.z; localTRS.m[9]  *= bone.localBindScale.z; localTRS.m[10] *= bone.localBindScale.z;

            if (bone.parentIndex < 0)
            {
                globalBindPoses[i] = localTRS;
            }
            else
            {
                globalBindPoses[i] = globalBindPoses[static_cast<usize>(bone.parentIndex)] * localTRS;
            }
        }

        // Invert each global bind pose to produce inverseBindPose
        // We use the fast affine inverse: InvAffine(M) = [Rt | -Rt*t]
        // where Rt is the transpose of the upper-left 3x3 (rotation+scale part).
        // For a TRS matrix this is exact and fast without a full 4x4 inverse.
        for (usize i = 0; i < m_bones.size(); ++i)
        {
            const Mat4& G = globalBindPoses[i];
            Mat4 inv = Mat4::Identity();

            // Upper-left 3x3 transpose (inverse rotation; valid when scale is uniform or decomposed)
            inv.m[0] = G.m[0]; inv.m[1] = G.m[4]; inv.m[2] = G.m[8];
            inv.m[4] = G.m[1]; inv.m[5] = G.m[5]; inv.m[6] = G.m[9];
            inv.m[8] = G.m[2]; inv.m[9] = G.m[6]; inv.m[10] = G.m[10];

            // Translation: -Rt * t
            inv.m[12] = -(inv.m[0] * G.m[12] + inv.m[4] * G.m[13] + inv.m[8]  * G.m[14]);
            inv.m[13] = -(inv.m[1] * G.m[12] + inv.m[5] * G.m[13] + inv.m[9]  * G.m[14]);
            inv.m[14] = -(inv.m[2] * G.m[12] + inv.m[6] * G.m[13] + inv.m[10] * G.m[14]);

            m_bones[i].inverseBindPose = inv;
        }
    }
}
