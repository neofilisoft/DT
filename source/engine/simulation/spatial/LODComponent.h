// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"
#include "core/string/StringID.h"
#include <array>

namespace lacrima::sim
{
    struct LODComponent
    {
        static constexpr u32 kMaxLODs = 4;
        std::array<f32, kMaxLODs - 1> distances = { 15.0f, 30.0f, 60.0f };
        std::array<lacrima::StringID, kMaxLODs> assetPaths;
        u8 currentLOD = 0;
        bool forceLOD = false;
        u8 forcedLODLevel = 0;

        REFLECT_BEGIN(LODComponent)
            REFLECT_FIELD(currentLOD)
            REFLECT_FIELD(forceLOD)
            REFLECT_FIELD(forcedLODLevel)
        REFLECT_END()
    };
}

