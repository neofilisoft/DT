// Copyright Neofilisoft. All Rights Reserved.
#include "simulation/spatial/LODSystem.h"
#include "simulation/world/SimulationWorld.h"
#include "core/logging/Logger.h"
#include <cmath>

namespace lacrima::sim
{
    void LODSystem::Step(SimulationWorld& world, const Vec3& cameraPos)
    {
        // Iterate over all entities that have an LODComponent
        world.LODs().ForEach([&](Entity entity, LODComponent& lod)
        {
            // Skip if forced LOD
            if (lod.forceLOD)
            {
                if (lod.currentLOD != lod.forcedLODLevel)
                {
                    lod.currentLOD = lod.forcedLODLevel;
                    if (VisualComponent* vis = world.Visuals().Get(entity))
                    {
                        vis->assetPath = lod.assetPaths[lod.currentLOD];
                    }
                }
                return;
            }

            // Calculate distance to camera
            TransformComponent* tr = world.Transforms().Get(entity);
            if (!tr) return; // No transform, can't calculate LOD

            f32 dx = tr->x - cameraPos.x;
            f32 dy = tr->y - cameraPos.y;
            f32 dz = tr->z - cameraPos.z;
            f32 distanceSq = (dx * dx) + (dy * dy) + (dz * dz);
            f32 distance = std::sqrt(distanceSq);

            // Determine LOD level
            u8 newLOD = 0;
            for (u8 l = 0; l < LODComponent::kMaxLODs - 1; ++l)
            {
                if (distance > lod.distances[l])
                {
                    newLOD = l + 1;
                }
                else
                {
                    break;
                }
            }

            // Apply new LOD if changed
            if (newLOD != lod.currentLOD)
            {
                lod.currentLOD = newLOD;
                if (VisualComponent* vis = world.Visuals().Get(entity))
                {
                    vis->assetPath = lod.assetPaths[newLOD];
                }
            }
        });
    }
}
