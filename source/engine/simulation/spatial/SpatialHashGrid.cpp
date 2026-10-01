// Copyright Neofilisoft. All Rights Reserved.
#include "simulation/spatial/SpatialHashGrid.h"
#include <cmath>

namespace lacrima::sim
{
    SpatialHashGrid::SpatialHashGrid(f32 cellSize)
        : m_cellSize(cellSize)
    {
    }

    SpatialHashGrid::CellPos SpatialHashGrid::GetCellPos(const TransformComponent& transform) const
    {
        return {
            static_cast<i32>(std::floor(transform.x / m_cellSize)),
            static_cast<i32>(std::floor(transform.z / m_cellSize))
        };
    }

    void SpatialHashGrid::Build(const ComponentArray<Entity, TransformComponent>& transforms,
                                const ComponentArray<Entity, InteractableComponent>& interactables)
    {
        m_cells.clear();
        
        // Only insert entities that are interactable
        interactables.ForEach([&](Entity entity, const InteractableComponent& /*interactable*/)
        {
            if (const TransformComponent* transform = transforms.Get(entity))
            {
                m_cells[GetCellPos(*transform)].push_back(entity);
            }
        });
    }

    std::vector<Entity> SpatialHashGrid::FindInRange(const TransformComponent& center, f32 radius, const ComponentArray<Entity, TransformComponent>& transforms) const
    {
        std::vector<Entity> result;
        
        // Calculate the bounding box of cells to check
        i32 minX = static_cast<i32>(std::floor((center.x - radius) / m_cellSize));
        i32 maxX = static_cast<i32>(std::floor((center.x + radius) / m_cellSize));
        i32 minZ = static_cast<i32>(std::floor((center.z - radius) / m_cellSize));
        i32 maxZ = static_cast<i32>(std::floor((center.z + radius) / m_cellSize));
        
        f32 radiusSq = radius * radius;
        
        for (i32 x = minX; x <= maxX; ++x)
        {
            for (i32 z = minZ; z <= maxZ; ++z)
            {
                CellPos pos{ x, z };
                auto it = m_cells.find(pos);
                if (it != m_cells.end())
                {
                    // Check exact distance for each entity in the cell
                    for (Entity entity : it->second)
                    {
                        if (const TransformComponent* transform = transforms.Get(entity))
                        {
                            f32 dx = transform->x - center.x;
                            f32 dz = transform->z - center.z;
                            if (dx * dx + dz * dz <= radiusSq)
                            {
                                result.push_back(entity);
                            }
                        }
                    }
                }
            }
        }
        
        return result;
    }
}

