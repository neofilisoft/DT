// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"
#include "runtime/Entity.h"
#include <functional>

namespace lacrima::sim
{
    /**
     * @brief A directed pair of entities, representing 'source's feeling towards 'target'.
     */
    struct DirectedPair
    {
        Entity source;
        Entity target;

        bool operator==(const DirectedPair& other) const
        {
            return source == other.source && target == other.target;
        }

        REFLECT_BEGIN(DirectedPair)
            REFLECT_FIELD(source)
            REFLECT_FIELD(target)
        REFLECT_END()
    };
}

namespace lacrima::sim
{
    /**
     * @brief A hash function for DirectedPair to use in unordered_map.
     */
    struct DirectedPairHasher
    {
        std::size_t operator()(const DirectedPair& p) const
        {
            std::size_t h1 = Entity::Hasher{}(p.source);
            std::size_t h2 = Entity::Hasher{}(p.target);
            return h1 ^ (h2 << 1);
        }
    };

    /**
     * @brief The scores and metadata tracking a relationship.
     */
    struct RelationshipScore
    {
        f32 friendship = 0.0f; // -100 to 100
        f32 romance = 0.0f;    // -100 to 100
        f32 rivalry = 0.0f;    // 0 to 100
        
        u64 lastInteractionTick = 0;

        REFLECT_BEGIN(RelationshipScore)
            REFLECT_FIELD(friendship)
            REFLECT_FIELD(romance)
            REFLECT_FIELD(rivalry)
            REFLECT_FIELD(lastInteractionTick)
        REFLECT_END()
    };
}

