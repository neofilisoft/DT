// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "runtime/Entity.h"
#include "simulation/spatial/TransformComponent.h"
#include "core/containers/ComponentArray.h"
#include "simulation/spatial/InteractableComponent.h"

#include <vector>
#include <unordered_map>
#include <functional>

namespace lacrima::sim
{
    // A simple 2D spatial hash grid using X and Z coordinates.
    // Optimizes spatial queries to O(1) cell lookups instead of O(N) iteration.
    class SpatialHashGrid
    {
    public:
        // cellSize determines the granularity of the grid. 
        // 5.0f or 10.0f are good default values for Sims-like spaces.
        explicit SpatialHashGrid(f32 cellSize = 10.0f);

        // Clears the grid and inserts all entities that have a Transform and Interactable component.
        // Should be called once per frame/tick before any AI or physics queries.
        void Build(const ComponentArray<Entity, TransformComponent>& transforms,
                   const ComponentArray<Entity, InteractableComponent>& interactables);

        // Finds all entities within the given radius around the center position.
        std::vector<Entity> FindInRange(const TransformComponent& center, f32 radius, const ComponentArray<Entity, TransformComponent>& transforms) const;

    private:
        struct CellPos
        {
            i32 x;
            i32 z;
            
            bool operator==(const CellPos& other) const
            {
                return x == other.x && z == other.z;
            }
        };

        struct CellPosHasher
        {
            std::size_t operator()(const CellPos& pos) const
            {
                // Simple hash combination for 2 integers
                return std::hash<i32>()(pos.x) ^ (std::hash<i32>()(pos.z) << 1);
            }
        };

        CellPos GetCellPos(const TransformComponent& transform) const;

        f32 m_cellSize;
        std::unordered_map<CellPos, std::vector<Entity>, CellPosHasher> m_cells;
    };
}

