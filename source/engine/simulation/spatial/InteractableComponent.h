// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "simulation/interaction/InteractionQueue.h"

namespace lacrima::sim
{
    // A component marking an entity as an interactable object in the world.
    // In a real archetype system, the InteractionTable would be shared per-archetype.
    // We embed it directly to allow each object to define its own
    // interactions, which AutonomySystem will query.
    struct InteractableComponent
    {
        InteractionTable interactions;
    };
}

