// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include "Relationship.h"
#include <unordered_map>
#include <vector>

namespace lacrima::sim
{
    class RelationshipGraph
    {
    public:
        /**
         * @brief Gets the relationship score from source to target.
         * Automatically applies time-based decay if currentTick is greater than lastInteractionTick.
         */
        RelationshipScore GetScore(Entity source, Entity target, u64 currentTick) const;

        /**
         * @brief Modifies a relationship score and resets the decay timer.
         */
        void AddInteraction(Entity source, Entity target, f32 friendshipDelta, f32 romanceDelta, f32 rivalryDelta, u64 currentTick);

        /**
         * @brief Cleans up any relationships involving the given entity.
         * Should be called when an entity is destroyed.
         */
        void RemoveEntity(Entity entity);

        // Serialization
        template <typename Writer>
        void Serialize(Writer& writer) const
        {
            writer.template WritePrimitive<u32>(static_cast<u32>(m_graph.size()));
            for (const auto& [pair, score] : m_graph)
            {
                writer.WriteObject(const_cast<DirectedPair*>(&pair), DirectedPair::StaticTypeInfo());
                writer.WriteObject(const_cast<RelationshipScore*>(&score), RelationshipScore::StaticTypeInfo());
            }
        }

        template <typename Reader>
        bool Deserialize(Reader& reader)
        {
            if (reader.AtEnd()) return false;
            m_graph.clear();
            
            u32 count = reader.template ReadPrimitive<u32>();
            for (u32 i = 0; i < count; ++i)
            {
                DirectedPair pair;
                RelationshipScore score;
                if (!reader.ReadObject(&pair, DirectedPair::StaticTypeInfo())) return false;
                if (!reader.ReadObject(&score, RelationshipScore::StaticTypeInfo())) return false;
                m_graph[pair] = score;
            }
            return true;
        }

    private:
        std::unordered_map<DirectedPair, RelationshipScore, DirectedPairHasher> m_graph;
        
        static constexpr f32 kDecayPerTick = 0.0005f; // Example tuning
    };
}



