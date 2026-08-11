#pragma once

#include "core/containers/ComponentArray.h"
#include "core/platform/Types.h"
#include "runtime/Entity.h"
#include "simulation/spatial/InteractableComponent.h"
#include "simulation/spatial/TransformComponent.h"

#include <vector>

namespace dt::sim
{
    class SpatialSystem
    {
    public:
        // Returns a list of all Entities that have an InteractableComponent and are within
        // `radius` distance of the `center` position.
        // This is a naive O(N) search for Milestone 9. Future milestones will replace this
        // with a spatial partition structure (QuadTree / Spatial Hashing).
        static std::vector<Entity> FindInteractablesInRange(
            const TransformComponent& center,
            f32 radius,
            const ComponentArray<Entity, TransformComponent>& transforms,
            const ComponentArray<Entity, InteractableComponent>& interactables);
            
        // Helper to calculate 2D distance (ignoring Y/Z for now, or XZ if 3D).
        // Using X and Z for horizontal plane distance.
        static f32 DistanceSq(const TransformComponent& a, const TransformComponent& b);
    };
}
