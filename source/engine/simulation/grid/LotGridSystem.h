// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "LotGridComponent.h"
#include "runtime/Entity.h"

namespace lacrima::sim
{
    class LotGridSystem
    {
    public:
        static void SetTileBlocked(LotGridComponent& grid, i32 x, i32 y, bool blocked);
        static void SetTileWalkable(LotGridComponent& grid, i32 x, i32 y, bool walkable);
        static void SetTileFloor(LotGridComponent& grid, i32 x, i32 y, u16 floorTextureId);
        static void SetRectOccupied(LotGridComponent& grid, i32 startX, i32 startY, i32 sizeX, i32 sizeY, bool occupied);
    };
}
