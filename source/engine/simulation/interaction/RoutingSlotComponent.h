// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "runtime/Entity.h"
#include "simulation/grid/IsometricCoord.h"
#include <vector>
#include <optional>

namespace lacrima::sim
{
    struct ObjectFootprint
    {
        i32 sizeX = 1;
        i32 sizeY = 1;
    };

    struct RoutingSlot
    {
        TilePos relativePos{ 0, 1 };
        bool isReserved = false;
        Entity reservedBy{};
    };

    struct RoutingSlotComponent
    {
        ObjectFootprint footprint;
        std::vector<RoutingSlot> slots;

        void AddSlot(i32 relX, i32 relY)
        {
            slots.push_back({ TilePos{ relX, relY }, false, Entity{} });
        }

        std::optional<TilePos> FindNearestAvailableSlot(TilePos objectRootTile, TilePos simTile) const
        {
            if (slots.empty())
            {
                return objectRootTile + TilePos{ 0, 1 };
            }

            std::optional<TilePos> bestSlot;
            i32 bestDist = 999999;

            for (const auto& slot : slots)
            {
                if (!slot.isReserved)
                {
                    TilePos worldSlot = objectRootTile + slot.relativePos;
                    i32 dist = worldSlot.ManhattanDistance(simTile);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestSlot = worldSlot;
                    }
                }
            }

            return bestSlot;
        }

        bool ReserveSlot(size_t slotIndex, Entity reserver)
        {
            if (slotIndex < slots.size() && !slots[slotIndex].isReserved)
            {
                slots[slotIndex].isReserved = true;
                slots[slotIndex].reservedBy = reserver;
                return true;
            }
            return false;
        }

        void ReleaseSlot(size_t slotIndex)
        {
            if (slotIndex < slots.size())
            {
                slots[slotIndex].isReserved = false;
                slots[slotIndex].reservedBy = Entity{};
            }
        }

        void ReleaseAllBy(Entity reserver)
        {
            for (auto& s : slots)
            {
                if (s.reservedBy == reserver)
                {
                    s.isReserved = false;
                    s.reservedBy = Entity{};
                }
            }
        }
    };
}
