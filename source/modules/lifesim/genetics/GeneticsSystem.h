// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include "GeneticProfile.h"

namespace lacrima::sim
{
    class GeneticsSystem
    {
    public:
        /**
         * @brief Combines two parent profiles into a child profile.
         * Deterministic based on the provided seed.
         */
        static GeneticProfile Inherit(const GeneticProfile& parent1, const GeneticProfile& parent2, u64 seed);
    };
}



