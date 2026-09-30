// Copyright Neofilisoft. All Rights Reserved.
#include "RelationshipGraph.h"
#include <cmath>
#include <algorithm>

namespace lacrima::sim
{
    RelationshipScore RelationshipGraph::GetScore(Entity source, Entity target, u64 currentTick) const
    {
        DirectedPair pair{source, target};
        auto it = m_graph.find(pair);
        if (it == m_graph.end())
        {
            return RelationshipScore{}; // Default 0
        }

        RelationshipScore score = it->second;
        
        // Lazy decay calculation
        if (currentTick > score.lastInteractionTick)
        {
            u64 deltaTicks = currentTick - score.lastInteractionTick;
            f32 decayAmount = static_cast<f32>(deltaTicks) * kDecayPerTick;

            // Decay towards 0
            auto decayTowardsZero = [](f32 val, f32 amount) -> f32
            {
                if (val > 0.0f) return std::max(0.0f, val - amount);
                if (val < 0.0f) return std::min(0.0f, val + amount);
                return 0.0f;
            };

            score.friendship = decayTowardsZero(score.friendship, decayAmount);
            score.romance = decayTowardsZero(score.romance, decayAmount);
            score.rivalry = decayTowardsZero(score.rivalry, decayAmount);
        }

        return score;
    }

    void RelationshipGraph::AddInteraction(Entity source, Entity target, f32 friendshipDelta, f32 romanceDelta, f32 rivalryDelta, u64 currentTick)
    {
        // First get the latest score with decay applied
        RelationshipScore score = GetScore(source, target, currentTick);

        // Apply deltas and clamp to limits
        score.friendship = std::clamp(score.friendship + friendshipDelta, -100.0f, 100.0f);
        score.romance = std::clamp(score.romance + romanceDelta, -100.0f, 100.0f);
        score.rivalry = std::clamp(score.rivalry + rivalryDelta, 0.0f, 100.0f);
        score.lastInteractionTick = currentTick;

        // Store back
        DirectedPair pair{source, target};
        m_graph[pair] = score;
    }

    void RelationshipGraph::RemoveEntity(Entity entity)
    {
        // Must erase any pair where source == entity OR target == entity
        for (auto it = m_graph.begin(); it != m_graph.end();)
        {
            if (it->first.source == entity || it->first.target == entity)
            {
                it = m_graph.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}



