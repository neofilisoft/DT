// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// VehicleComponent.h - ECS Component (Step 6)
//
// Attached to a 3D entity to make it a driveable vehicle.
// Owns the IVehicleController* (created via IPhysicsSystem::CreateVehicleController).
// VehicleSystem reads driver input and copies vehicle transform -> TransformComponent.
//
// Lifetime: VehicleSystem creates the controller on first Tick if m_controller is null
// and the entity has a TransformComponent. It destroys it via m_physicsSystem on
// SimulationWorld::DestroyEntity or Stop (PIE shutdown).
// ---------------------------------------------------------------------------

#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"
#include <physics/IVehicleController.h>
#include <physics/PhysicsTypes.h>
#include <memory>

namespace lacrima::sim
{
    struct VehicleComponent
    {
        // --- Vehicle shape parameters (authored in editor) ---
        glm::vec3 halfExtents{1.0f, 0.35f, 2.2f};  // Body AABB half-extents (m)
        float     mass             = 1500.0f;        // kg
        float     maxEngineTorque  = 500.0f;         // Nm
        float     maxBrakeTorque   = 1500.0f;        // Nm
        float     maxSteerAngle    = 0.5236f;        // ~30 deg (rad)
        int       differentialType = 1;              // 0=FWD, 1=RWD, 2=AWD

        // --- Runtime driver input (set by input handler or AI each tick) ---
        float inputForward   = 0.0f;   // -1..+1
        float inputRight     = 0.0f;   // -1..+1
        float inputBrake     = 0.0f;   //  0..+1
        float inputHandBrake = 0.0f;   //  0..+1

        // --- Runtime state (filled by VehicleSystem) ---
        float speedKmh = 0.0f;

        // --- Owned physics controller (set by VehicleSystem, not serialized) ---
        lacrima::physics::IVehicleController* controller = nullptr;  // raw, owned by IPhysicsSystem

        // --- Serialized wheel count (for re-creating wheels on load) ---
        u32 wheelCount = 4;  // Standard car = 4

        REFLECT_BEGIN(VehicleComponent)
            REFLECT_FIELD(halfExtents)
            REFLECT_FIELD(mass)
            REFLECT_FIELD(maxEngineTorque)
            REFLECT_FIELD(maxBrakeTorque)
            REFLECT_FIELD(maxSteerAngle)
            REFLECT_FIELD(differentialType)
            REFLECT_FIELD(wheelCount)
            REFLECT_FIELD(speedKmh)
        REFLECT_END()
    };
}
