#pragma once

#include "core/platform/Types.h"

// ---------------------------------------------------------------------------
// ScriptStepResult.h
//
// The outcome of one ScriptCoroutine::Step() call. Originally placed under
// simulation/interaction/ (as "InteractionStepResult") during initial
// design, which was a dependency-direction mistake: this enum describes
// what happened when a Lua coroutine was resumed - a scripting-layer
// concept - not anything intrinsically about interactions or simulation.
// Homing it here means scripting/ has zero dependency on simulation/,
// matching the intended DAG (core -> runtime -> scripting -> simulation).
// simulation/interaction/InteractionQueue.h uses this type directly (via
// `using InteractionStepResult = script::ScriptStepResult;` for
// call-site-friendly naming) rather than redeclaring an equivalent enum of
// its own.
// ---------------------------------------------------------------------------

namespace dt::script
{
    // Named "CONTINUE" not "Running" deliberately - mirrors the
    // documented SimAntics primitive return convention ({TRUE, FALSE,
    // CONTINUE ON NEXT TICK}) for a multi-tick action that yields control
    // back to the scheduler between ticks (see ScriptCoroutine.h for full
    // rationale on implementing this via a real Lua coroutine).
    enum class ScriptStepResult : u8
    {
        Continue,
        Complete,
        Failed
    };
}
