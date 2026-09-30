// Copyright Neofilisoft. All Rights Reserved.
#pragma once
#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"
#include "Trait.h"
#include <array>

namespace lacrima::sim
{
    /**
     * @brief Stores genetic traits and physical descriptors for an entity.
     */
    struct GeneticProfile
    {
        u32 activeTraits = static_cast<u32>(Trait::None);
        
        f32 skinTone = 0.5f;
        f32 eyeColor = 0.5f;
        f32 hairColor = 0.5f;
        f32 bodyBuild = 0.5f;

        REFLECT_BEGIN(GeneticProfile)
            REFLECT_FIELD(activeTraits)
            REFLECT_FIELD(skinTone)
            REFLECT_FIELD(eyeColor)
            REFLECT_FIELD(hairColor)
            REFLECT_FIELD(bodyBuild)
        REFLECT_END()
    };
}



