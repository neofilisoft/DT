// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include <physics/IPhysicsBody.h>

namespace lacrima::physics
{
    // Attaching this component to an Entity in SimulationWorld 
    // gives it a physical presence in the world.
    struct PhysicsComponent
    {
        IPhysicsBody* body = nullptr;
    };
}

