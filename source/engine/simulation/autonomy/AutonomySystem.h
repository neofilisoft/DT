#pragma once

#include "core/platform/Types.h"
#include "runtime/Entity.h"
#include "simulation/interaction/InteractionQueue.h"
#include "simulation/needs/NeedsComponent.h"

#include <array>
#include <functional>
#include <vector>

// ---------------------------------------------------------------------------
// AutonomySystem.h
//
// DTEngine's own utility-based autonomy scoring, connecting NeedsComponent
// to InteractionQueue. This is what runs when a Sim's InteractionQueue is
// empty (the documented "idle for input with allow push" moment - see
// InteractionQueue.h's file comment) and something must be chosen
// automatically.
//
// Design, informed by the general "autonomy desire" concept documented
// for the Sims lineage (Lot51's Commodity docs: low commodities create
// desire for interactions that restore them) but built as DTEngine's own
// scoring formula rather than any specific implementation:
//
//   score(interaction) = sum over each need this interaction satisfies of:
//       (need.autonomyWeight) * UrgencyCurve(currentValue, convergence, failureThreshold)
//     + interaction.basePriority
//
// UrgencyCurve is deliberately nonlinear (quadratic in how far the need
// has drifted from convergence, not linear) - this is a specific design
// choice, not something read off any reference implementation: a need at
// 90% of its way to failure should be picked dramatically more urgently
// than one at 10%, not just proportionally more, or a sim would dither
// between many mildly-low needs instead of decisively addressing whichever
// one is closest to actually failing. This also naturally reproduces the
// commonly observed behavior (documented across community motive guides)
// that a Sim "single-mindedly" pursues food once Hunger gets critically
// low, without any special-cased "if critical, force this" branch - it
// falls out of the curve shape alone.
//
// AutonomySystem itself does not know what a "Fridge" or "Bed" is. It is
// handed a list of currently-available (Entity target, InteractionDef)
// candidates (produced by whatever spatial/interaction-discovery system
// finds nearby usable objects - not yet built; out of scope for this
// module) and picks the highest-scoring one. This keeps AutonomySystem
// itself completely game-content-agnostic, matching the "engine must
// avoid game-specific dependencies" rule.
// ---------------------------------------------------------------------------

namespace dt::sim
{
    // What one candidate interaction restores, expressed as (which need,
    // how much it's worth satisfying per use) - lets AutonomySystem score
    // a candidate without needing to actually run it first. Content data
    // (populated per InteractionDef by the game layer / a .asset), not
    // computed by the engine.
    struct NeedSatisfaction
    {
        NeedId need;
        f32 satisfactionAmount = 0.0f; // how much this interaction moves the need toward convergence, used only for scoring weight
    };

    struct AutonomyCandidate
    {
        const InteractionDef* def = nullptr;
        Entity target;
        std::vector<NeedSatisfaction> satisfies;
    };

    // Nonlinear urgency in [0, 1]: 0 = need is at or past convergence
    // (fully satisfied, no urgency), 1 = need is at or past
    // failureThreshold (maximally urgent). Quadratic falloff - see file
    // comment for why linear was rejected.
    f32 ComputeUrgency(f32 currentValue, const NeedDefinition& definition);

    // Scores every candidate against the entity's current NeedsComponent
    // and returns the highest-scoring one, or nullptr if `candidates` is
    // empty. Pure function - no side effects, does not push anything onto
    // any queue itself, so it is trivially unit-testable and safe to call
    // from within a TaskGraph node without touching shared queue state
    // directly (the caller, e.g. the Autonomy TaskGraph node's per-entity
    // step, is responsible for actually calling
    // InteractionQueue::PushAutonomous with the result).
    const AutonomyCandidate* SelectBestCandidate(
        const NeedsComponent& needs,
        const std::array<NeedDefinition, kNeedCount>& definitions,
        const std::vector<AutonomyCandidate>& candidates);
}
