#include "simulation/spatial/SpatialSystem.h"

namespace dt::sim
{
    f32 SpatialSystem::DistanceSq(const TransformComponent& a, const TransformComponent& b)
    {
        // Assuming Y is up, XZ is the ground plane for distance calculations
        const f32 dx = a.x - b.x;
        const f32 dz = a.z - b.z;
        return (dx * dx) + (dz * dz);
    }

    std::vector<Entity> SpatialSystem::FindInteractablesInRange(
        const TransformComponent& center,
        f32 radius,
        const ComponentArray<Entity, TransformComponent>& transforms,
        const ComponentArray<Entity, InteractableComponent>& interactables)
    {
        std::vector<Entity> result;
        const f32 radiusSq = radius * radius;

        // Iterate over all interactables
        interactables.ForEach([&](Entity entity, const InteractableComponent& /*interactable*/)
        {
            // Check if it has a transform
            if (const TransformComponent* transform = transforms.Get(entity))
            {
                if (DistanceSq(center, *transform) <= radiusSq)
                {
                    result.push_back(entity);
                }
            }
        });

        return result;
    }
}
