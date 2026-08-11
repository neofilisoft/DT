#pragma once

#include "runtime/Entity.h"
#include "scripting/ScriptEngine.h"
#include "scripting/ScriptStepResult.h"

#include <sol/sol.hpp>

#include <optional>
#include <string>

// ---------------------------------------------------------------------------
// ScriptCoroutine.h
//
// Wraps a Lua coroutine (sol::coroutine) for a single in-flight
// interaction's run-function, exposing only Step()/IsFinished() -
// simulation/ code (InteractionQueue::StepFront, see InteractionQueue.h)
// depends on this type but never touches sol:: directly, per
// ScriptEngine.h's boundary discipline.
//
// Lua-side contract for an interaction run-function (matches
// InteractionDef::luaRunFunction, see InteractionQueue.h):
//
//   function SomeInteraction_Run(actor, target, dt)
//       -- do first-tick work
//       coroutine.yield("continue")   -- pause here; resumes next Step() call
//       -- do more work across as many ticks as needed
//       return "complete"             -- or "failed"
//   end
//
// Each ScriptCoroutine::Step() call performs exactly one Lua
// coroutine.resume with fresh (actor, target, dt) arguments, matching the
// documented SimAntics-inspired "CONTINUE ON NEXT TICK" semantics from
// InteractionQueue.h's file comment - the difference being this is a real
// Lua coroutine (debuggable, real control flow) rather than a bespoke
// instruction-pointer-persistence mechanism.
// ---------------------------------------------------------------------------

namespace dt::script
{
    class ScriptCoroutine
    {
    public:
        // Looks up `functionName` as a global Lua function and wraps it as
        // a fresh coroutine. Returns std::nullopt if the function doesn't
        // exist - callers (InteractionQueue::StepFront) treat a missing
        // run-function as an immediate Failed result rather than crashing,
        // since a content/data error (a typo'd luaRunFunction name in an
        // InteractionDef) must not be able to take down the simulation
        // thread.
        static std::optional<ScriptCoroutine> Create(ScriptEngine& engine, const std::string& functionName);

        ScriptCoroutine(const ScriptCoroutine&) = delete;
        ScriptCoroutine& operator=(const ScriptCoroutine&) = delete;
        ScriptCoroutine(ScriptCoroutine&&) = default;
        ScriptCoroutine& operator=(ScriptCoroutine&&) = default;

        // Resumes the coroutine with (actor, target, dt) and maps the
        // yielded/returned Lua value ("continue"/"complete"/"failed") to
        // ScriptStepResult. A Lua runtime error during resume,
        // or an unrecognized return string, is treated as Failed (logged).
        // `actor`/`target` are passed through as the Entity usertype
        // (registered by SimulationWorld, see SimulationWorld.cpp's
        // RegisterLuaBindings) so Lua-side code can pass them straight
        // into bound API calls like dt.get_need(actor, "Hunger") without
        // any bit-packing/reconstruction glue.
        ScriptStepResult Step(Entity actor, Entity target, f32 dt);

        bool IsFinished() const { return m_finished; }

    private:
        ScriptCoroutine(sol::thread runner, sol::coroutine coroutine) 
            : m_runner(std::move(runner)), m_coroutine(std::move(coroutine)) {}

        sol::thread m_runner;
        sol::coroutine m_coroutine;
        bool m_finished = false;
    };
}
