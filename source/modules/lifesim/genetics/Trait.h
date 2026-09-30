// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include "core/platform/Types.h"

namespace lacrima::sim
{
    /**
     * @brief Personality traits that affect behavior and autonomy weighting.
     * Stored as a bitmask.
     */
    enum class Trait : u32
    {
        None        = 0,
        Curious     = 1 << 0,
        Lazy        = 1 << 1,
        Charismatic = 1 << 2,
        Slob        = 1 << 3,
        Neat        = 1 << 4,
        Athletic    = 1 << 5,
        Clumsy      = 1 << 6,
        Genius      = 1 << 7
    };

    inline constexpr Trait operator|(Trait a, Trait b)
    {
        return static_cast<Trait>(static_cast<u32>(a) | static_cast<u32>(b));
    }

    inline constexpr Trait operator&(Trait a, Trait b)
    {
        return static_cast<Trait>(static_cast<u32>(a) & static_cast<u32>(b));
    }

    inline constexpr Trait& operator|=(Trait& a, Trait b)
    {
        a = a | b;
        return a;
    }

    inline constexpr Trait& operator&=(Trait& a, Trait b)
    {
        a = a & b;
        return a;
    }

    inline constexpr bool HasTrait(u32 mask, Trait t)
    {
        return (mask & static_cast<u32>(t)) != 0;
    }
}

