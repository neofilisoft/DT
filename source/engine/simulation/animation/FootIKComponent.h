// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// FootIKComponent.h - ECS Component (Step 6)
//
// Stores the per-entity state for 2-bone foot IK.
// FootIKSystem reads this component plus SkeletalMeshComponent bone transforms
// and outputs adjusted foot positions / rotations for the left and right feet.
//
// IK algorithm: 2-bone (FABRIK-style analytic):
//   Given thigh-knee-ankle chain lengths L1, L2 and a target position T,
//   the analytic solution gives the unique bend direction satisfying
//   |thigh->ankle| = dist(T, hip). The foot is then rotated to align its
//   sole normal with the surface normal retrieved by a downward raycast.
//
// Raycast is performed each tick via IPhysicsSystem::CastRay from above
// the foot's world-space position downward. If no hit or target is too far
// (exceeds maxReachDistance), the IK blends out (blendWeight -> 0).
// ---------------------------------------------------------------------------

#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace lacrima::sim
{
    struct FootIKTarget
    {
        glm::vec3 worldPosition{0.0f}; // Raycast target (ground hit point)
        glm::vec3 groundNormal{0.0f, 1.0f, 0.0f}; // Surface normal at contact
        float     blendWeight = 0.0f;  // 0=IK off, 1=IK fully active
        bool      hasHit      = false;
    };

    struct FootIKComponent
    {
        // --- Bone name bindings (must match skeleton joint names) ---
        std::string leftThighBone  = "LeftUpLeg";
        std::string leftKneeBone   = "LeftLeg";
        std::string leftAnkleBone  = "LeftFoot";

        std::string rightThighBone = "RightUpLeg";
        std::string rightKneeBone  = "RightLeg";
        std::string rightAnkleBone = "RightFoot";

        // --- Parameters ---
        float raycastLength      = 1.5f;  // How far downward to cast (m) from foot position
        float raycastOriginUp    = 0.5f;  // Start ray this far above foot position (m)
        float maxReachDistance   = 0.6f;  // If ground is further than this, IK blends out (m)
        float blendSpeed         = 8.0f;  // Blend weight change speed (per second)
        float footRotationSpeed  = 10.0f; // Foot normal rotation smoothing (per second)
        float pelvisAdjustSpeed  = 5.0f;  // Speed at which pelvis shifts down to reach both feet

        // --- Runtime state (computed by FootIKSystem) ---
        FootIKTarget leftFoot{};
        FootIKTarget rightFoot{};
        float        pelvisYOffset = 0.0f; // Applied to entity TransformComponent.position.y

        REFLECT_BEGIN(FootIKComponent)
            REFLECT_FIELD(leftThighBone)
            REFLECT_FIELD(leftKneeBone)
            REFLECT_FIELD(leftAnkleBone)
            REFLECT_FIELD(rightThighBone)
            REFLECT_FIELD(rightKneeBone)
            REFLECT_FIELD(rightAnkleBone)
            REFLECT_FIELD(raycastLength)
            REFLECT_FIELD(raycastOriginUp)
            REFLECT_FIELD(maxReachDistance)
            REFLECT_FIELD(blendSpeed)
        REFLECT_END()
    };
}
