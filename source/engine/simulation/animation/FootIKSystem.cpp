// Copyright Neofilisoft. All Rights Reserved.
#include "FootIKSystem.h"
#include "simulation/world/SimulationWorld.h"
#include <physics/IPhysicsSystem.h>
#include <physics/PhysicsTypes.h>
#include <core/math/Frustum.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace lacrima::sim
{
    // -----------------------------------------------------------------------
    // Helper: find bone index by name in SkeletalMeshComponent
    // Returns -1 if not found.
    // -----------------------------------------------------------------------
    static i32 FindBoneIndex(const SkeletalMeshComponent& smc, const std::string& boneName)
    {
        if (!smc.skeleton) return -1;
        return smc.skeleton->FindBoneIndex(boneName);
    }

    // -----------------------------------------------------------------------
    // Helper: get world-space bone position from the current animated pose.
    // currentPose.globalTransforms[idx] is the joint's global matrix.
    // We extract translation (column 3).
    // -----------------------------------------------------------------------
    static glm::vec3 GetBoneWorldPos(const AnimationPose& pose, i32 boneIdx, const glm::mat4& entityWorld)
    {
        if (boneIdx < 0 || boneIdx >= static_cast<i32>(pose.globalTransforms.size()))
            return glm::vec3(0.0f);
        glm::vec4 localPos = pose.globalTransforms[boneIdx][3]; // column 3 = translation
        return glm::vec3(entityWorld * localPos);
    }

    // -----------------------------------------------------------------------
    // Helper: rotate a quaternion so that 'fromDir' aligns with 'toDir'
    // -----------------------------------------------------------------------
    static glm::quat RotateToAlign(const glm::vec3& fromDir, const glm::vec3& toDir)
    {
        glm::vec3 f = glm::normalize(fromDir);
        glm::vec3 t = glm::normalize(toDir);
        float dot = glm::dot(f, t);
        dot = glm::clamp(dot, -1.0f, 1.0f);
        if (dot >= 0.9999f) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 axis = glm::cross(f, t);
        float sinHalf = std::sqrt((1.0f - dot) * 0.5f);
        float cosHalf = std::sqrt((1.0f + dot) * 0.5f);
        return glm::normalize(glm::quat(cosHalf, axis * sinHalf));
    }

    // -----------------------------------------------------------------------
    // 2-Bone Analytic IK Solver
    //
    // Inputs:
    //   thighWorld  - joint A position (world)
    //   kneeWorld   - joint B position (world, used for bend hint extraction)
    //   ankleWorld  - joint C position (world)
    //   target      - desired ankle world position
    //   bendHint    - preferred plane normal for knee bend (e.g. forward dir)
    //
    // Outputs:
    //   outKneeWorld  - solved knee position
    //   outAnkleWorld - solved ankle position (== target if reachable)
    //
    // Returns false if chain is at full stretch (no unique solution).
    // -----------------------------------------------------------------------
    bool FootIKSystem::SolveTwoBone(const glm::vec3& thighWorld,
                                     const glm::vec3& kneeWorld,
                                     const glm::vec3& ankleWorld,
                                     const glm::vec3& target,
                                     const glm::vec3& bendHint,
                                     glm::vec3& outKneeWorld,
                                     glm::vec3& outAnkleWorld)
    {
        const float L1  = glm::length(kneeWorld  - thighWorld); // thigh segment
        const float L2  = glm::length(ankleWorld - kneeWorld);  // shin segment

        glm::vec3 toTarget = target - thighWorld;
        float D = glm::length(toTarget);

        // Clamp to max reach
        const float maxReach = L1 + L2 - 1e-4f;
        if (D < 1e-5f)
        {
            outKneeWorld  = kneeWorld;
            outAnkleWorld = ankleWorld;
            return false;
        }

        bool fullyStretched = (D >= maxReach);
        if (fullyStretched) D = maxReach;

        // Law of cosines: angle at A (thigh)
        // cos(alpha) = (L1^2 + D^2 - L2^2) / (2 * L1 * D)
        float cosAlpha = glm::clamp((L1 * L1 + D * D - L2 * L2) / (2.0f * L1 * D), -1.0f, 1.0f);
        float alpha    = std::acos(cosAlpha);

        // Knee bend direction: cross(toTarget, bendHint), then cross back to get plane normal
        glm::vec3 toTargetNorm = glm::normalize(toTarget);

        // Prefer the existing knee position as bend hint source
        glm::vec3 currentKneeDir = glm::normalize(kneeWorld - thighWorld);
        glm::vec3 planeNormal    = glm::cross(toTargetNorm, currentKneeDir);
        if (glm::dot(planeNormal, planeNormal) < 1e-6f)
            planeNormal = glm::cross(toTargetNorm, bendHint);
        if (glm::dot(planeNormal, planeNormal) < 1e-6f)
            planeNormal = glm::vec3(0.0f, 0.0f, 1.0f); // fallback
        planeNormal = glm::normalize(planeNormal);

        // Knee bend direction (perpendicular to toTarget in the bend plane)
        glm::vec3 bendDir = glm::normalize(glm::cross(planeNormal, toTargetNorm));

        // Solved knee: rotate thigh->target by alpha around bendDir
        glm::vec3 kneeDir  = toTargetNorm * std::cos(alpha) + bendDir * std::sin(alpha);
        outKneeWorld  = thighWorld + kneeDir * L1;
        outAnkleWorld = fullyStretched ? (thighWorld + toTargetNorm * maxReach)
                                       : target;
        return true;
    }

    // -----------------------------------------------------------------------
    // Update - process all entities with FootIKComponent + SkeletalMeshComponent
    // -----------------------------------------------------------------------
    void FootIKSystem::Update(SimulationWorld& world, float dt)
    {
        auto* physics = world.GetPhysicsSystem();
        if (!physics) return;

        auto& footIKs    = world.FootIKs();
        auto& skeletons  = world.SkeletalMeshes();
        auto& transforms = world.Transforms();

        footIKs.ForEach([&](Entity entity, FootIKComponent& fik)
        {
            SkeletalMeshComponent* smc = skeletons.Get(entity);
            TransformComponent*    tc  = transforms.Get(entity);
            if (!smc || !tc || !smc->skeleton) return;

            // --- Build entity world matrix ---
            glm::mat4 entityWorld = glm::mat4(1.0f);
            entityWorld = glm::translate(entityWorld, glm::vec3(tc->x, tc->y, tc->z));
            entityWorld *= glm::mat4_cast(glm::quat(glm::vec3(0.0f, tc->yaw, 0.0f)));
            entityWorld = glm::scale(entityWorld, glm::vec3(1.0f));

            // Entity forward direction for knee bend hint
            glm::vec3 entityForward = glm::normalize(glm::vec3(glm::quat(glm::vec3(0.0f, tc->yaw, 0.0f)) * glm::vec4(0,0,1,0)));
            glm::vec3 entityUp      = glm::vec3(0,1,0);

            // Process left and right feet
            struct FootData {
                const std::string& thighBone;
                const std::string& kneeBone;
                const std::string& ankleBone;
                FootIKTarget&      target;
            };

            FootData feet[2] = {
                { fik.leftThighBone,  fik.leftKneeBone,  fik.leftAnkleBone,  fik.leftFoot  },
                { fik.rightThighBone, fik.rightKneeBone, fik.rightAnkleBone, fik.rightFoot }
            };

            float minPelvisDelta = 0.0f;  // Most negative foot delta

            for (auto& foot : feet)
            {
                i32 thighIdx = FindBoneIndex(*smc, foot.thighBone);
                i32 kneeIdx  = FindBoneIndex(*smc, foot.kneeBone);
                i32 ankleIdx = FindBoneIndex(*smc, foot.ankleBone);

                if (thighIdx < 0 || kneeIdx < 0 || ankleIdx < 0) continue;

                glm::vec3 thighW = GetBoneWorldPos(smc->currentPose, thighIdx, entityWorld);
                glm::vec3 kneeW  = GetBoneWorldPos(smc->currentPose, kneeIdx,  entityWorld);
                glm::vec3 ankleW = GetBoneWorldPos(smc->currentPose, ankleIdx, entityWorld);

                // --- Raycast downward from above ankle ---
                glm::vec3 rayOrigin = ankleW + entityUp * fik.raycastOriginUp;
                glm::vec3 rayDir    = -entityUp;
                physics::RaycastHit hit{};
                bool gotHit = physics->CastRay(rayOrigin, rayDir, fik.raycastLength + fik.raycastOriginUp, hit);

                // --- Blend weight update ---
                float targetBlend = 0.0f;
                if (gotHit)
                {
                    float delta = glm::length(hit.hitPoint - ankleW);
                    targetBlend = (delta <= fik.maxReachDistance) ? 1.0f : 0.0f;
                }
                foot.target.blendWeight = glm::mix(foot.target.blendWeight,
                                                    targetBlend,
                                                    glm::clamp(fik.blendSpeed * dt, 0.0f, 1.0f));
                foot.target.hasHit = gotHit;

                if (!gotHit || foot.target.blendWeight < 0.01f) continue;

                foot.target.worldPosition = hit.hitPoint;
                foot.target.groundNormal  = hit.hitNormal;

                // --- 2-Bone IK ---
                glm::vec3 solvedKnee, solvedAnkle;
                SolveTwoBone(thighW, kneeW, ankleW,
                             hit.hitPoint, entityForward,
                             solvedKnee, solvedAnkle);

                // --- Apply IK (blend) into the pose globalTransforms ---
                // Write back ankle global position (lerp by blendWeight)
                if (ankleIdx < static_cast<i32>(smc->currentPose.globalTransforms.size()))
                {
                    glm::vec3 blended = glm::mix(ankleW, solvedAnkle, foot.target.blendWeight);
                    smc->currentPose.globalTransforms[ankleIdx][3] = glm::vec4(blended, 1.0f);
                }
                // Write back knee
                if (kneeIdx < static_cast<i32>(smc->currentPose.globalTransforms.size()))
                {
                    glm::vec3 blended = glm::mix(kneeW, solvedKnee, foot.target.blendWeight);
                    smc->currentPose.globalTransforms[kneeIdx][3] = glm::vec4(blended, 1.0f);
                }

                // Track pelvis adjustment
                float footDelta = hit.hitPoint.y - ankleW.y;
                minPelvisDelta = std::min(minPelvisDelta, footDelta);
            }

            // --- Pelvis adjustment (smooth) ---
            float targetPelvisY = std::min(minPelvisDelta, 0.0f); // Only lower, never raise
            fik.pelvisYOffset = glm::mix(fik.pelvisYOffset,
                                          targetPelvisY,
                                          glm::clamp(fik.pelvisAdjustSpeed * dt, 0.0f, 1.0f));
            tc->y += fik.pelvisYOffset;
        });
    }
}



