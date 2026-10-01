// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "runtime/Entity.h"
#include "simulation/grid/IsometricCoord.h"
#include <vector>
#include <algorithm>

namespace lacrima::sim
{
    struct SortedSpriteEntry
    {
        Entity entity;
        i64 depthKey = 0;
        i32 tileX = 0;
        i32 tileY = 0;
        i32 layer = 0; // 0 = Terrain/Floor, 1 = Rugs, 2 = Furniture/Sims, 3 = Overhead/Roofs
    };

    class IsometricDepthSorter
    {
    public:
        static constexpr i64 kLayerMultiplier = 1000000LL;
        static constexpr i64 kTileMultiplier  = 1000LL;

        // Calculates composite depth key for 2.5D Isometric ordering
        static i64 CalculateDepthKey(i32 tileX, i32 tileY, i32 layer = 2, i32 subOrder = 0)
        {
            return static_cast<i64>(layer) * kLayerMultiplier +
                   static_cast<i64>(tileX + tileY) * kTileMultiplier +
                   static_cast<i64>(subOrder);
        }

        // Sorts entries ascending (back-to-front rendering order)
        static void Sort(std::vector<SortedSpriteEntry>& entries);
    };
}
