// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// FootIKSystem.h (Step 6)
//
// ECS System for 2-bone analytical foot IK with ground contact.
//
// Algorithm per foot (left and right, symmetric):
//   1. RAYCAST: Cast a ray downward from above the animated ankle position.
//      If no hit -> blend weight decreases toward 0.
//   2. 2-BONE IK: Given thigh (A), knee (B), ankle (C) and target T:
//      a. Compute L1 = |AB|, L2 = |BC|, D = |AT|.
//      b. Clamp D <= L1+L2 (clamped reach).
//      c. Use law of cosines to find angle at knee.
//      d. Project T into the plane of the chain, compute knee hint direction
//         (forward-perpendicular) to determine the unique bend direction.
//      e. Reconstruct B' = A + L1 * normalize(boneDir), C' = B' + L2 * normalize(B'->T).
//   3. FOOT ROTATION: Rotate the ankle joint so the sole normal aligns with
//      the ground hit normal (slerp over footRotationSpeed).
//   4. PELVIS ADJUSTMENT: Lower the entity root by pelvisYOffset = min(leftDelta, rightDelta)
//      so neither foot floats above the ground.
// ---------------------------------------------------------------------------

#include "simulation/animation/FootIKComponent.h"
#include "simulation/animation/SkeletalMeshComponent.h"

namespace lacrima::sim
{
    class SimulationWorld;

    class FootIKSystem
    {
    public:
        static void Update(SimulationWorld& world, float dt);

    private:
        // Solve a single 2-bone chain, returning the adjusted ankle world position.
        // thighWorld / kneeWorld / ankleWorld are the current global-space positions.
        // bendHint is the preferred knee-bend direction (usually forward cross up).
        static bool SolveTwoBone(const glm::vec3& thighWorld,
                                  const glm::vec3& kneeWorld,
                                  const glm::vec3& ankleWorld,
                                  const glm::vec3& target,
                                  const glm::vec3& bendHint,
                                  glm::vec3& outKneeWorld,
                                  glm::vec3& outAnkleWorld);
    };
}
