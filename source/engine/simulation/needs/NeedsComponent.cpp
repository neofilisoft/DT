#include "simulation/needs/NeedsComponent.h"

#include <algorithm>
#include <cmath>

namespace dt::sim
{
    const char* ToString(NeedId id)
    {
        switch (id)
        {
            case NeedId::Hunger:      return "Hunger";
            case NeedId::Bladder:     return "Bladder";
            case NeedId::Energy:      return "Energy";
            case NeedId::Hygiene:     return "Hygiene";
            case NeedId::Social:      return "Social";
            case NeedId::Fun:         return "Fun";
            case NeedId::Comfort:     return "Comfort";
            case NeedId::Environment: return "Environment";
            default:                  return "Invalid";
        }
    }

    void DecayNeeds(NeedsComponent& component, const std::array<NeedDefinition, kNeedCount>& definitions, f32 fixedDeltaSeconds)
    {
        for (usize i = 0; i < kNeedCount; ++i)
        {
            const NeedDefinition& def = definitions[i];
            f32& value = component.values[i];

            // Convergence-based decay: derive the sign from which side of
            // `convergence` the current value sits on, rather than
            // hardcoding "needs always fall". A need whose convergence is
            // above its current value rises over time (e.g. a "Fatness"
            // stat with a high default convergence would rise toward it);
            // one whose convergence is below the current value falls
            // (e.g. Hunger, whose convergence is typically 0 - "fully
            // fed" is the natural resting state the sim drifts away from
            // as time passes, in the direction of needing food again).
            const f32 direction = (def.convergence > value) ? 1.0f : (def.convergence < value) ? -1.0f : 0.0f;
            value += direction * def.decayRatePerSecond * fixedDeltaSeconds;

            // Clamp so decay never overshoots convergence (would cause
            // oscillation) and never exceeds the definition's declared
            // range.
            if (direction > 0.0f)
            {
                value = std::min(value, def.convergence);
            }
            else if (direction < 0.0f)
            {
                value = std::max(value, def.convergence);
            }

            value = std::clamp(value, def.minValue, def.maxValue);
        }
    }

    void ApplyNeedDelta(NeedsComponent& component, const NeedDefinition& definition, NeedId id, f32 delta)
    {
        const usize index = static_cast<usize>(id);
        component.values[index] = std::clamp(component.values[index] + delta, definition.minValue, definition.maxValue);
    }
}
