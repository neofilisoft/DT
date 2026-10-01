// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"
#include "IsometricCoord.h"
#include <vector>

namespace lacrima::sim
{
    enum class TileFlag : u8
    {
        Empty    = 0,
        Walkable = 1 << 0,
        Blocked  = 1 << 1,
        HasWall  = 1 << 2,
        Occupied = 1 << 3
    };

    inline TileFlag operator|(TileFlag a, TileFlag b) { return static_cast<TileFlag>(static_cast<u8>(a) | static_cast<u8>(b)); }
    inline TileFlag operator&(TileFlag a, TileFlag b) { return static_cast<TileFlag>(static_cast<u8>(a) & static_cast<u8>(b)); }
    inline TileFlag& operator|=(TileFlag& a, TileFlag b) { a = a | b; return a; }

    struct LotGridComponent
    {
        u32 width = 32;
        u32 height = 32;
        f32 tileWidth = 64.0f;
        f32 tileHeight = 32.0f;

        std::vector<u8>  flags;
        std::vector<u16> floorTextureIds;
        std::vector<u16> roomIds;

        void Initialize(u32 w, u32 h, f32 tWidth = 64.0f, f32 tHeight = 32.0f)
        {
            width = w;
            height = h;
            tileWidth = tWidth;
            tileHeight = tHeight;
            const size_t total = static_cast<size_t>(width * height);
            flags.assign(total, static_cast<u8>(TileFlag::Walkable));
            floorTextureIds.assign(total, 0);
            roomIds.assign(total, 0);
        }

        bool InBounds(i32 x, i32 y) const
        {
            return x >= 0 && x < static_cast<i32>(width) && y >= 0 && y < static_cast<i32>(height);
        }

        size_t ToIndex(i32 x, i32 y) const
        {
            return static_cast<size_t>(y * width + x);
        }

        u8 GetFlags(i32 x, i32 y) const
        {
            if (!InBounds(x, y)) return static_cast<u8>(TileFlag::Blocked);
            return flags[ToIndex(x, y)];
        }

        void SetFlags(i32 x, i32 y, u8 flagMask)
        {
            if (InBounds(x, y)) flags[ToIndex(x, y)] = flagMask;
        }

        void AddFlag(i32 x, i32 y, TileFlag flag)
        {
            if (InBounds(x, y)) flags[ToIndex(x, y)] |= static_cast<u8>(flag);
        }

        void RemoveFlag(i32 x, i32 y, TileFlag flag)
        {
            if (InBounds(x, y)) flags[ToIndex(x, y)] &= ~static_cast<u8>(flag);
        }

        bool IsWalkable(i32 x, i32 y) const
        {
            if (!InBounds(x, y)) return false;
            u8 f = flags[ToIndex(x, y)];
            bool isBlocked = (f & (static_cast<u8>(TileFlag::Blocked) | static_cast<u8>(TileFlag::HasWall) | static_cast<u8>(TileFlag::Occupied))) != 0;
            bool isWalkable = (f & static_cast<u8>(TileFlag::Walkable)) != 0;
            return isWalkable && !isBlocked;
        }

        REFLECT_BEGIN(LotGridComponent)
            REFLECT_FIELD(width)
            REFLECT_FIELD(height)
            REFLECT_FIELD(tileWidth)
            REFLECT_FIELD(tileHeight)
        REFLECT_END()
    };
}
