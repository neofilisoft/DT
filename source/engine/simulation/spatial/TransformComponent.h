// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"

namespace lacrima::sim
{
    struct TransformComponent
    {
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 z = 0.0f;
        f32 yaw = 0.0f; // in radians

        REFLECT_BEGIN(TransformComponent)
            REFLECT_FIELD(x)
            REFLECT_FIELD(y)
            REFLECT_FIELD(z)
            REFLECT_FIELD(yaw)
        REFLECT_END()
    };
}

