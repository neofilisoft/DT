#pragma once

#include "core/platform/Types.h"
#include "simulation/interaction/InteractionQueue.h"

namespace dt::sim
{
    // A component marking an entity as an interactable object in the world.
    // In a real archetype system, the InteractionTable would be shared per-archetype.
    // For Milestone 9, we embed it directly to allow each object to define its own
    // interactions, which AutonomySystem will query.
    struct InteractableComponent
    {
        InteractionTable interactions;
    };
}
