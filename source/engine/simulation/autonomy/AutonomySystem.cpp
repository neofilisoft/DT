#include "simulation/autonomy/AutonomySystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dt::sim
{
    f32 ComputeUrgency(f32 currentValue, const NeedDefinition& definition)
    {
        const f32 totalRange = std::abs(definition.convergence - definition.failureThreshold);
        if (totalRange < 1e-6f)
        {
            return 0.0f; // Degenerate definition (convergence == failureThreshold); no meaningful urgency curve.
        }

        // Normalized distance traveled from convergence toward
        // failureThreshold, in [0, 1]. Works regardless of whether
        // failureThreshold is below convergence (e.g. Hunger: convergence
        // near max, failure near 0) or above it (a hypothetical need
        // whose failure state is "too high", not "too low") since we take
        // the distance along whichever direction failureThreshold
        // actually lies in.
        const f32 distanceFromConvergence = std::abs(currentValue - definition.convergence);
        const f32 normalized = std::clamp(distanceFromConvergence / totalRange, 0.0f, 1.0f);

        // Quadratic, not linear - see AutonomySystem.h file comment for
        // why: makes near-failure needs dominate scoring disproportionately
        // rather than merely proportionally.
        return normalized * normalized;
    }

    const AutonomyCandidate* SelectBestCandidate(
        const NeedsComponent& needs,
        const std::array<NeedDefinition, kNeedCount>& definitions,
        const std::vector<AutonomyCandidate>& candidates)
    {
        const AutonomyCandidate* best = nullptr;
        f32 bestScore = -std::numeric_limits<f32>::infinity();

        for (const AutonomyCandidate& candidate : candidates)
        {
            f32 score = candidate.def != nullptr ? candidate.def->basePriority : 0.0f;

            for (const NeedSatisfaction& satisfaction : candidate.satisfies)
            {
                const usize needIndex = static_cast<usize>(satisfaction.need);
                const NeedDefinition& def = definitions[needIndex];
                const f32 currentValue = needs.Get(satisfaction.need);
                const f32 urgency = ComputeUrgency(currentValue, def);

                score += def.autonomyWeight * urgency;
            }

            if (score > bestScore)
            {
                bestScore = score;
                best = &candidate;
            }
        }

        return best;
    }
}
