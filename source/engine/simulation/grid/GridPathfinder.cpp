// Copyright Neofilisoft. All Rights Reserved.
#include "GridPathfinder.h"
#include <queue>
#include <unordered_map>
#include <cmath>
#include <algorithm>

namespace lacrima::sim
{
    struct AStarNode
    {
        TilePos pos;
        f32 gCost = 0.0f;
        f32 fCost = 0.0f;

        bool operator>(const AStarNode& other) const
        {
            return fCost > other.fCost;
        }
    };

    f32 GridPathfinder::Heuristic(TilePos a, TilePos b, bool diagonal)
    {
        f32 dx = static_cast<f32>(std::abs(a.x - b.x));
        f32 dy = static_cast<f32>(std::abs(a.y - b.y));

        if (!diagonal)
        {
            return dx + dy;
        }

        // Octile distance
        constexpr f32 kSqrt2Minus2 = 0.41421356f;
        return (dx < dy) ? (kSqrt2Minus2 * dx + dy) : (kSqrt2Minus2 * dy + dx);
    }

    PathResult GridPathfinder::FindPath(
        const LotGridComponent& grid,
        TilePos start,
        TilePos goal,
        bool allowDiagonal,
        u32 maxSearchIterations)
    {
        PathResult result;

        if (start == goal)
        {
            result.found = true;
            result.path.push_back(start);
            return result;
        }

        if (!grid.InBounds(start.x, start.y) || !grid.InBounds(goal.x, goal.y))
        {
            return result;
        }

        if (!grid.IsWalkable(goal.x, goal.y))
        {
            return result;
        }

        std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;
        std::unordered_map<TilePos, f32, TilePosHasher> gCosts;
        std::unordered_map<TilePos, TilePos, TilePosHasher> cameFrom;

        openSet.push({ start, 0.0f, Heuristic(start, goal, allowDiagonal) });
        gCosts[start] = 0.0f;

        const TilePos orthogonalOffsets[4] = {
            { 0,  1 }, // North
            { 0, -1 }, // South
            { 1,  0 }, // East
            {-1,  0 }  // West
        };

        const TilePos diagonalOffsets[4] = {
            { 1,  1 }, // North-East
            {-1,  1 }, // North-West
            { 1, -1 }, // South-East
            {-1, -1 }  // South-West
        };

        u32 iterations = 0;
        bool goalReached = false;

        while (!openSet.empty() && iterations++ < maxSearchIterations)
        {
            AStarNode current = openSet.top();
            openSet.pop();

            if (current.pos == goal)
            {
                goalReached = true;
                break;
            }

            // If we found a cheaper way to current.pos already, skip
            if (current.gCost > gCosts[current.pos])
            {
                continue;
            }

            // 1. Orthogonal neighbors
            for (const auto& offset : orthogonalOffsets)
            {
                TilePos neighbor = current.pos + offset;

                if (!grid.IsWalkable(neighbor.x, neighbor.y))
                {
                    continue;
                }

                f32 tentativeG = current.gCost + 1.0f;
                auto it = gCosts.find(neighbor);
                if (it == gCosts.end() || tentativeG < it->second)
                {
                    cameFrom[neighbor] = current.pos;
                    gCosts[neighbor] = tentativeG;
                    f32 fCost = tentativeG + Heuristic(neighbor, goal, allowDiagonal);
                    openSet.push({ neighbor, tentativeG, fCost });
                }
            }

            // 2. Diagonal neighbors
            if (allowDiagonal)
            {
                for (const auto& offset : diagonalOffsets)
                {
                    TilePos neighbor = current.pos + offset;

                    // Corner-cutting check: both adjacent orthogonal tiles must be walkable
                    if (!grid.IsWalkable(current.pos.x + offset.x, current.pos.y) ||
                        !grid.IsWalkable(current.pos.x, current.pos.y + offset.y))
                    {
                        continue;
                    }

                    if (!grid.IsWalkable(neighbor.x, neighbor.y))
                    {
                        continue;
                    }

                    f32 tentativeG = current.gCost + 1.41421356f;
                    auto it = gCosts.find(neighbor);
                    if (it == gCosts.end() || tentativeG < it->second)
                    {
                        cameFrom[neighbor] = current.pos;
                        gCosts[neighbor] = tentativeG;
                        f32 fCost = tentativeG + Heuristic(neighbor, goal, allowDiagonal);
                        openSet.push({ neighbor, tentativeG, fCost });
                    }
                }
            }
        }

        if (goalReached)
        {
            result.found = true;
            TilePos curr = goal;
            while (curr != start)
            {
                result.path.push_back(curr);
                curr = cameFrom[curr];
            }
            result.path.push_back(start);
            std::reverse(result.path.begin(), result.path.end());
        }

        return result;
    }
}
