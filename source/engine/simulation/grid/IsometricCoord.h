// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/math/Math.h"
#include <cmath>

namespace lacrima::sim
{
    struct TilePos
    {
        i32 x = 0;
        i32 y = 0;

        bool operator==(const TilePos& other) const { return x == other.x && y == other.y; }
        bool operator!=(const TilePos& other) const { return !(*this == other); }
        TilePos operator+(const TilePos& o) const { return { x + o.x, y + o.y }; }
        TilePos operator-(const TilePos& o) const { return { x - o.x, y - o.y }; }

        i32 ManhattanDistance(const TilePos& o) const
        {
            return std::abs(x - o.x) + std::abs(y - o.y);
        }
    };

    struct TilePosHasher
    {
        size_t operator()(const TilePos& pos) const
        {
            return (static_cast<size_t>(pos.x) * 73856093u) ^ (static_cast<size_t>(pos.y) * 19349663u);
        }
    };

    enum class WorldRotation : u8
    {
        North = 0, // 0 deg
        East  = 1, // 90 deg
        South = 2, // 180 deg
        West  = 3  // 270 deg
    };

    class IsometricCoord
    {
    public:
        // Converts discrete grid tile (tileX, tileY) to continuous 2.5D screen coordinates
        static Vec2 TileToScreen(i32 tileX, i32 tileY, f32 tileWidth = 64.0f, f32 tileHeight = 32.0f, Vec2 origin = { 0.0f, 0.0f })
        {
            f32 halfW = tileWidth * 0.5f;
            f32 halfH = tileHeight * 0.5f;

            f32 sx = static_cast<f32>(tileX - tileY) * halfW + origin.x;
            f32 sy = static_cast<f32>(tileX + tileY) * halfH + origin.y;
            return Vec2(sx, sy);
        }

        // Converts continuous 2.5D screen coordinates back into discrete grid tile (tileX, tileY)
        static TilePos ScreenToTile(Vec2 screenPos, f32 tileWidth = 64.0f, f32 tileHeight = 32.0f, Vec2 origin = { 0.0f, 0.0f })
        {
            f32 halfW = tileWidth * 0.5f;
            f32 halfH = tileHeight * 0.5f;

            f32 dx = screenPos.x - origin.x;
            f32 dy = screenPos.y - origin.y;

            f32 fx = (dx / halfW + dy / halfH) * 0.5f;
            f32 fy = (dy / halfH - dx / halfW) * 0.5f;

            return TilePos{
                static_cast<i32>(std::floor(fx + 0.5f)),
                static_cast<i32>(std::floor(fy + 0.5f))
            };
        }

        // Rotates tile coordinates according to lot rotation
        static TilePos RotateTile(TilePos pos, u32 lotWidth, u32 lotHeight, WorldRotation rot)
        {
            switch (rot)
            {
                case WorldRotation::North: return pos;
                case WorldRotation::East:  return TilePos{ pos.y, static_cast<i32>(lotWidth - 1) - pos.x };
                case WorldRotation::South: return TilePos{ static_cast<i32>(lotWidth - 1) - pos.x, static_cast<i32>(lotHeight - 1) - pos.y };
                case WorldRotation::West:  return TilePos{ static_cast<i32>(lotHeight - 1) - pos.y, pos.x };
            }
            return pos;
        }
    };
}
