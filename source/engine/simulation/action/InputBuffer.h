// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include <array>

namespace lacrima::action
{
    // Frame-perfect input tracking for fighting/action games
    struct InputBuffer
    {
        static constexpr usize kBufferSize = 10;
        
        // Bitmask of inputs for each of the last kBufferSize frames
        std::array<u64, kBufferSize> history = {0};
        
        void PushFrame(u64 currentInputs)
        {
            for (usize i = kBufferSize - 1; i > 0; --i)
            {
                history[i] = history[i - 1];
            }
            history[0] = currentInputs;
        }
        
        // Checks if an input was pressed within the last window frames
        bool WasInputBuffered(u64 inputMask, u32 windowFrames) const
        {
            windowFrames = windowFrames < kBufferSize ? windowFrames : kBufferSize;
            for (u32 i = 0; i < windowFrames; ++i)
            {
                if ((history[i] & inputMask) == inputMask) return true;
            }
            return false;
        }
    };
}
