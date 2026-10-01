// Copyright Neofilisoft. All Rights Reserved.
#include <physics/PhysicsEngine.h>
#include "jolt/JoltPhysicsSystem.h"
#include "box2d/Box2DPhysicsSystem.h"
#include <core/platform/Assert.h>

namespace lacrima::physics
{
    std::unique_ptr<IPhysicsSystem> PhysicsEngine::CreatePhysicsSystem(PhysicsBackend backend)
    {
        switch (backend)
        {
        case PhysicsBackend::Jolt3D:
            return std::make_unique<JoltPhysicsSystem>();
        case PhysicsBackend::Box2D:
            return std::make_unique<Box2DPhysicsSystem>();
        }
        LACRIMA_UNREACHABLE("Unhandled PhysicsBackend enum case");
        return nullptr;
    }
}


