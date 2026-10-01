// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/string/StringID.h"

namespace lacrima::action
{
    // Base class for animation-driven events (like Unreal AnimNotifies)
    struct AnimNotify
    {
        StringID eventName;
        float triggerTime; // Normalised time [0.0 - 1.0] or frame number
        bool fired = false;
        
        virtual ~AnimNotify() = default;
        virtual void Execute(u64 entityId) = 0;
    };
}
