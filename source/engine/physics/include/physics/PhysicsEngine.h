// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include <physics/PhysicsTypes.h>
#include <physics/IPhysicsSystem.h>
#include <memory>

namespace lacrima::physics
{
    class PhysicsEngine
    {
    public:
        static std::unique_ptr<IPhysicsSystem> CreatePhysicsSystem(PhysicsBackend backend);
    };
}

