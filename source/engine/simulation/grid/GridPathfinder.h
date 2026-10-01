// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "LotGridComponent.h"
#include <vector>

namespace lacrima::sim
{
    struct PathResult
    {
        bool found = false;
        std::vector<TilePos> path;
    };

    class GridPathfinder
    {
    public:
        // Discrete A* pathfinding on LotGridComponent
        static PathResult FindPath(
            const LotGridComponent& grid,
            TilePos start,
            TilePos goal,
            bool allowDiagonal = false,
            u32 maxSearchIterations = 4096
        );

    private:
        static f32 Heuristic(TilePos a, TilePos b, bool diagonal);
    };
}
