// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/containers/ComponentArray.h"
#include "runtime/Entity.h"
#include "simulation/spatial/TransformComponent.h"
#include "simulation/spatial/SpringArmComponent.h"

namespace lacrima::physics
{
    class IPhysicsSystem;
}

namespace lacrima::sim
{
    class SpringArmSystem
    {
    public:
        // Updates all spring arm components in the world:
        // 1. Damps yaw and pitch based on camera lag
        // 2. Computes ray from character target offset towards camera
        // 3. Sweeps SphereCast against Jolt physics
        // 4. Retracts camera arm dynamically to prevent geometry clipping
        static void Update(
            f32 deltaTime,
            ComponentArray<Entity, TransformComponent>& transforms,
            ComponentArray<Entity, SpringArmComponent>& springArms,
            physics::IPhysicsSystem* physicsSystem = nullptr);
    };
}