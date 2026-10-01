// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// VehicleSystem.h (Step 6)
//
// ECS System that manages vehicle physics lifecycle and syncs state:
//   1. On first tick per entity: creates IVehicleController via IPhysicsSystem,
//      builds a standard 4-wheel layout from VehicleComponent parameters.
//   2. Every tick: forwards driver input -> controller->SetDriverInput(),
//      copies controller position/rotation -> TransformComponent.
//   3. On PIE Stop / entity destroy: calls IPhysicsSystem::DestroyVehicleController.
//
// VehicleSystem::Update is wired into SimulationWorld's tick graph AFTER
// the physics Step node (so vehicle bodies are already stepped by Jolt
// before we read back their transform).
// ---------------------------------------------------------------------------

#include "simulation/animation/VehicleComponent.h"
#include <physics/IVehicleController.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lacrima::sim
{
    class SimulationWorld;
    class VehicleSystem
    {
    public:
        // Build 4-wheel layout from VehicleComponent fields.
        // Called internally when controller == nullptr.
        static physics::VehicleDesc BuildDefaultCarDesc(const VehicleComponent& comp,
                                                         const glm::vec3& spawnPos,
                                                         const glm::quat& spawnRot);

        // Main tick - iterates all entities with VehicleComponent + TransformComponent.
        static void Update(SimulationWorld& world, float dt);

        // Cleanup a single entity's vehicle controller (call on DestroyEntity / PIE Stop).
        static void DestroyController(SimulationWorld& world, Entity entity);
    };
}

