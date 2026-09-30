// Copyright Neofilisoft. All Rights Reserved.
#include "GeneticsSystem.h"
#include <random>
#include <algorithm>

namespace lacrima::sim
{
    GeneticProfile GeneticsSystem::Inherit(const GeneticProfile& parent1, const GeneticProfile& parent2, u64 seed)
    {
        GeneticProfile child;
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<f32> dist(0.0f, 1.0f);
        std::uniform_int_distribution<u32> traitDist(0, 7);

        // Mix physical traits: mostly average with slight randomization
        auto mix = [&](f32 a, f32 b) -> f32
        {
            f32 r = dist(rng);
            f32 base = (r < 0.45f) ? a : (r < 0.90f) ? b : ((a + b) * 0.5f);
            
            // 10% chance of slight mutation
            if (dist(rng) < 0.1f)
            {
                base += (dist(rng) - 0.5f) * 0.2f;
            }
            return std::clamp(base, 0.0f, 1.0f);
        };

        child.skinTone = mix(parent1.skinTone, parent2.skinTone);
        child.eyeColor = mix(parent1.eyeColor, parent2.eyeColor);
        child.hairColor = mix(parent1.hairColor, parent2.hairColor);
        child.bodyBuild = mix(parent1.bodyBuild, parent2.bodyBuild);

        // Mix active traits: union of parents, randomly dropping some to avoid trait bloat
        u32 combinedTraits = parent1.activeTraits | parent2.activeTraits;
        u32 inheritedTraits = 0;

        for (u32 i = 0; i < 8; ++i)
        {
            u32 mask = (1 << i);
            if ((combinedTraits & mask) != 0)
            {
                // 60% chance to inherit a trait that at least one parent has
                if (dist(rng) < 0.60f)
                {
                    inheritedTraits |= mask;
                }
            }
        }

        // 10% chance for a completely random mutation trait
        if (dist(rng) < 0.10f)
        {
            u32 randomTrait = (1 << traitDist(rng));
            inheritedTraits |= randomTrait;
        }

        child.activeTraits = inheritedTraits;
        return child;
    }
}



