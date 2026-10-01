// Copyright Neofilisoft. All Rights Reserved.
#include "LotGridSystem.h"

namespace lacrima::sim
{
    void LotGridSystem::SetTileBlocked(LotGridComponent& grid, i32 x, i32 y, bool blocked)
    {
        if (blocked)
            grid.AddFlag(x, y, TileFlag::Blocked);
        else
            grid.RemoveFlag(x, y, TileFlag::Blocked);
    }

    void LotGridSystem::SetTileWalkable(LotGridComponent& grid, i32 x, i32 y, bool walkable)
    {
        if (walkable)
            grid.AddFlag(x, y, TileFlag::Walkable);
        else
            grid.RemoveFlag(x, y, TileFlag::Walkable);
    }

    void LotGridSystem::SetTileFloor(LotGridComponent& grid, i32 x, i32 y, u16 floorTextureId)
    {
        if (grid.InBounds(x, y))
        {
            grid.floorTextureIds[grid.ToIndex(x, y)] = floorTextureId;
        }
    }

    void LotGridSystem::SetRectOccupied(LotGridComponent& grid, i32 startX, i32 startY, i32 sizeX, i32 sizeY, bool occupied)
    {
        for (i32 dy = 0; dy < sizeY; ++dy)
        {
            for (i32 dx = 0; dx < sizeX; ++dx)
            {
                i32 x = startX + dx;
                i32 y = startY + dy;
                if (occupied)
                    grid.AddFlag(x, y, TileFlag::Occupied);
                else
                    grid.RemoveFlag(x, y, TileFlag::Occupied);
            }
        }
    }
}
