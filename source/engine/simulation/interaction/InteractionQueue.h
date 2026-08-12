#pragma once

#include "core/handle/Handle.h"
#include "core/platform/Types.h"
#include "runtime/Entity.h"
#include "scripting/ScriptCoroutine.h"
#include "scripting/ScriptStepResult.h"

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// InteractionQueue.h
//
// DTEngine's own interaction system. Conceptually informed by the publicly
// documented TSO/SimAntics "Tree Table" (TTAB) and interaction-queue
// pattern (see the Volcanic academic paper's description: named
// interactions with an "action" and "check" tree, queued via a player's
// Pie Menu or the "Push Interaction" primitive, drained one at a time from
// an object's own instruction pointer) - but implemented completely
// differently, for a concrete reason:
//
// TSO's BHAV/TTAB system exists because the original game shipped no
// source, only compiled behavior trees players could never see - object
// "scripts" had to be data (a tree of numbered OpCodes) that the shipped
// binary could interpret, because there was no way to ship and compile
// real code per-object at runtime. DTEngine has no such constraint: we
// embed Lua (locked in during the architecture discussion specifically
// for gameplay/interaction scripting) which is a real, debuggable
// scripting language with actual control flow, so building a bespoke
// bytecode VM to replicate that legacy constraint would be a pure
// regression - more code, worse debuggability, for a problem we don't
// have. DTEngine's InteractionDef stores a C++/reflected "check" predicate
// and a Lua "run" coroutine-style callback directly, with no intermediate
// tree-of-opcodes representation at all.
//
// What's preserved from the documented concept (because it's a genuinely
// good pattern, not a legacy workaround):
//   - interactions are declared per-object-Archetype as a named table,
//     not hardcoded per-object-instance logic (InteractionTable below,
//     playing the TTAB role)
//   - a "check" predicate gates whether an interaction is even offered,
//     evaluated before it's queued or shown to the player (CanRun below)
//   - queued interactions drain one at a time from a per-entity FIFO,
//     shared between player-initiated and autonomy-initiated pushes (see
//     AutonomySystem.h) - this is the actual valuable idea: player intent
//     and AI intent go through the identical execution path, so there is
//     no special-cased "AI does X differently than a player command"
//     branch anywhere in the engine
// ---------------------------------------------------------------------------

namespace dt::sim
{
    using Entity = dt::Entity;

    // Alias, not a redeclaration - the actual enum is script::ScriptStepResult
    // (see scripting/ScriptStepResult.h for why it's homed there: it
    // describes a Lua coroutine's step outcome, a scripting-layer concept,
    // not something simulation-specific). Kept as this name here purely
    // for call-site readability in interaction code ("this interaction
    // step Failed" reads more clearly than "this ScriptStepResult was
    // Failed" at InteractionQueue/AutonomySystem call sites).
    using InteractionStepResult = script::ScriptStepResult;

    struct InteractionDef
    {
        std::string name;
        std::string luaCheckFunction;  // Lua global function name: bool(Entity actor, Entity target)
        std::string luaRunFunction;    // Lua coroutine function name: InteractionStepResult(Entity actor, Entity target, f32 dt)
        f32 basePriority = 0.0f;       // used for autonomy ranking, see AutonomySystem.h
        bool playerVisible = true;     // false = autonomy-only (e.g. "React to fire") interactions never shown in a pie-menu-equivalent UI
    };

    // Per-Archetype interaction table - the TTAB analogue. Registered once
    // per object archetype (Fridge, Bed, TV, ...), shared by every
    // instance of that archetype, matching "Fridge:Interact(sim)" /
    // "Bed:Sleep(sim)" style dispatch already established for this
    // project's C++/Lua split.
    class InteractionTable
    {
    public:
        void Register(InteractionDef def);
        const InteractionDef* Find(const std::string& name) const;
        const std::vector<InteractionDef>& All() const { return m_interactions; }

    private:
        std::vector<InteractionDef> m_interactions;
    };

    // One queued instance of an interaction being run against a specific
    // target by a specific actor. Distinct from InteractionDef (which is
    // the shared, per-archetype *definition*) the same way a class is
    // distinct from an instance.
    struct QueuedInteraction
    {
        const InteractionDef* def = nullptr;
        Entity target;
        bool isAutonomous = false; // true if AutonomySystem pushed this rather than a player/UI action
    };

    // Per-entity FIFO of queued interactions. One instance lives inside
    // each Sim-capable entity's own component set (owned by the
    // Interaction module's ComponentArray<Entity, InteractionQueueComponent>,
    // not embedded here - this class is the queue's own logic,
    // independent of how/where it's stored per-entity).
    class InteractionQueue
    {
    public:
        // Player-initiated or explicitly scripted push: always goes to
        // the back of the queue, preserving FIFO order for anything
        // already queued (matches "Push Interaction" primitive semantics
        // as documented: new pushes append, they don't preempt what's
        // already running).
        void Push(const InteractionDef& def, Entity target);

        // Autonomy-initiated push (see AutonomySystem.h): also FIFO
        // append, not priority-insert-to-front. DTEngine's Autonomy
        // system is expected to only push when the queue is empty (the
        // documented "idle for input with allow push" pattern: autonomy
        // only offers a new choice when the sim has nothing else to do),
        // so this queue class itself stays simple - it does not implement
        // interruption/priority-preemption of an already-running
        // interaction. If Domaintic's design later needs a Sim to
        // interrupt a running interaction for something urgent (e.g. a
        // fire alarm), that is a deliberate future extension
        // (InteractionQueue::Interrupt), not assumed here.
        void PushAutonomous(const InteractionDef& def, Entity target);

        bool IsEmpty() const { return m_queue.empty(); }
        usize Size() const { return m_queue.size(); }

        // Returns the interaction currently at the front (being executed
        // or about to start), or nullptr if the queue is empty.
        QueuedInteraction* Front();

        // Removes the front interaction (call after it reports Complete
        // or Failed).
        void PopFront();

        void Clear() { m_queue.clear(); }

        // Advances the front interaction by exactly one tick, driving its
        // real Lua run-function via ScriptCoroutine (see ScriptCoroutine.h
        // for the yield/return contract). The front interaction's
        // actual luaRunFunction executes, potentially across multiple
        // ticks via coroutine.yield("continue").
        //
        // On IsEmpty(), does nothing and returns Failed (callers should
        // check IsEmpty() first; this return value exists so the function
        // has a well-defined result in that case rather than being
        // undefined behavior to call on an empty queue).
        //
        // On Complete or Failed, automatically pops the front entry and
        // discards its coroutine state before returning - callers do not
        // need to call PopFront() themselves after StepFront reports
        // completion, unlike the old instant-resolution path which
        // required an explicit PopFront call.
        InteractionStepResult StepFront(script::ScriptEngine& engine, Entity actor, f32 fixedDeltaSeconds);

    private:
        std::deque<QueuedInteraction> m_queue;

        // Coroutine state for whichever interaction is currently at
        // m_queue.front(). Only ever holds a value corresponding to the
        // CURRENT front entry - reset (and a new one lazily created on the
        // next StepFront call) whenever the front entry changes, whether
        // via PopFront/Clear or because StepFront itself popped a
        // completed interaction. Stored as std::optional rather than
        // inside QueuedInteraction itself because ScriptCoroutine is
        // move-only (wraps a sol::coroutine, which is not copyable) while
        // QueuedInteraction is a plain copyable value type stored directly
        // in m_queue's std::deque - keeping coroutine state external
        // avoids forcing every QueuedInteraction to become move-only for
        // the sake of the (at most one) entry that's actually executing.
        std::optional<script::ScriptCoroutine> m_frontCoroutine;
        const InteractionDef* m_frontCoroutineOwner = nullptr; // which InteractionDef m_frontCoroutine belongs to, to detect front-changed
    };
}
