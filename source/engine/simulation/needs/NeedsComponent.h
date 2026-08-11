#pragma once

#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"

#include <array>
#include <string_view>

// ---------------------------------------------------------------------------
// NeedsComponent.h
//
// DTEngine's own generic decaying-stat system. Conceptually informed by the
// publicly documented "Commodity" concept used across the Sims lineage
// (see Lot51's Sims 4 modding docs and community motive documentation:
// a stat with min/max/convergence/decay-rate, decaying toward its
// convergence point over time, driving autonomy desire) - but the design
// below is DTEngine's own, built from that general, widely-documented
// pattern rather than any specific engine's source code. Two concrete
// differences from what's documented for the Sims lineage:
//
//   1. A single generic Decay() function operates on every need
//      uniformly via NeedDefinition data (convergence + rate), rather than
//      hardcoded per-need decay logic. Sims titles reportedly special-case
//      certain motives (bladder failure triggers a hygiene failure,
//      hunger failure triggers death) - DTEngine keeps decay purely
//      generic/data-driven here and handles failure *consequences*
//      (what happens at 0) as a separate, explicit FailureRule table, not
//      baked into the decay function itself. This keeps NeedsComponent
//      reusable for a life-sim, a colony-sim, or any other DTEngine
//      project without the decay math itself knowing what "bladder"
//      means.
//
//   2. Needs are identified by a small fixed enum (NeedId) with
//      reflected f32 fields per need, NOT a dynamic named-commodity
//      registry. This trades runtime extensibility (Sims 4's Commodities
//      are a moddable, ID-based system) for reflection/serialization
//      simplicity and cache-friendly fixed-size storage appropriate for a
//      1-3 developer team's content scale (a fixed, curated need list is
//      easier to balance and ship than an open-ended commodity registry).
//      If Domaintic's design later needs moddable custom needs, that is a
//      deliberate future extension point, not something copied from an
//      existing implementation now.
//
// Convergence-based decay (not simple linear decrement) is the key
// property this generalizes: each need has a `convergence` value it drifts
// toward, and `decayRatePerSecond` is always stored positive with sign
// applied dynamically based on current value vs convergence - this is
// what let the historical design support needs that *rise* over time
// without a separate "growth" code path (matches the publicly documented
// Commodity behavior).
// ---------------------------------------------------------------------------

namespace dt::sim
{
    enum class NeedId : u8
    {
        Hunger = 0,
        Bladder,
        Energy,
        Hygiene,
        Social,
        Fun,
        Comfort,
        Environment,
        Count
    };

    inline constexpr usize kNeedCount = static_cast<usize>(NeedId::Count);

    // Tuning data for one need. Lives in game-layer content (loaded via
    // Config or a .asset, not hardcoded in engine code) - engine only
    // defines the shape.
    struct NeedDefinition
    {
        f32 minValue = 0.0f;
        f32 maxValue = 100.0f;
        f32 convergence = 100.0f;         // value decay drifts toward
        f32 decayRatePerSecond = 0.1f;    // always positive; sign derived from (current vs convergence) each tick
        f32 failureThreshold = 0.0f;      // at/below this, a FailureRule (if any) fires
        f32 autonomyWeight = 1.0f;        // relative urgency multiplier used by AutonomySystem's utility scoring

        REFLECT_BEGIN(NeedDefinition)
            REFLECT_FIELD(minValue)
            REFLECT_FIELD(maxValue)
            REFLECT_FIELD(convergence)
            REFLECT_FIELD(decayRatePerSecond)
            REFLECT_FIELD(failureThreshold)
            REFLECT_FIELD(autonomyWeight)
        REFLECT_END()
    };

    // Per-entity live need values. Fixed-size array indexed by NeedId,
    // not a dynamic map - see file comment for why. REFLECT_FIELD_ARRAY
    // isn't used here (this is a fixed std::array, not a std::vector) so
    // it gets its own small manual reflection registration below instead
    // of going through the generic macro path, since std::array<f32, N>
    // doesn't fit either REFLECT_FIELD (not a recognized scalar/struct
    // type) or REFLECT_FIELD_ARRAY (which specifically targets
    // std::vector<T>).
    struct NeedsComponent
    {
        std::array<f32, kNeedCount> values{};

        f32 Get(NeedId id) const { return values[static_cast<usize>(id)]; }
        void Set(NeedId id, f32 value) { values[static_cast<usize>(id)] = value; }
    };

    const char* ToString(NeedId id);

    // Applies one tick of convergence-based decay to every need in
    // `component`, using per-need tuning from `definitions`. Pure
    // function of (component, definitions, dt) - no hidden state, no RNG,
    // no clock reads - matching SimulationLoop's determinism contract
    // (see runtime/SimulationLoop.h) so this can run identically inside a
    // TaskGraph node regardless of which worker thread executes it.
    void DecayNeeds(NeedsComponent& component, const std::array<NeedDefinition, kNeedCount>& definitions, f32 fixedDeltaSeconds);

    // Applies an immediate need change from satisfying an interaction
    // (e.g. eating restores Hunger toward its convergence). Clamped to
    // [minValue, maxValue] from the corresponding NeedDefinition.
    void ApplyNeedDelta(NeedsComponent& component, const NeedDefinition& definition, NeedId id, f32 delta);
}
