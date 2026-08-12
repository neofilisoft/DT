#pragma once

#include "core/platform/Types.h"
#include "runtime/SimulationLoop.h"

// ---------------------------------------------------------------------------
// SimClock.h
//
// The Time module - first stage in the per-tick pipeline
// (Time -> Needs -> Relationship -> AI -> Job Queue -> Navigation ->
// Animation State -> Object State). Per the M0 architecture discussion:
// keep exactly one real clock (tick count, monotonic, what determinism and
// replay actually key off) and derive everything presentational
// (in-game day/hour/minute) FROM it, rather than maintaining tick count
// and calendar time as two parallel clocks that could drift relative to
// each other.
//
// SimClock itself does nothing per tick except record the current
// tickIndex/fixedDeltaSeconds SimulationLoop already hands to every
// TaskGraph node this tick (see SimulationWorld::BuildTickGraph) - it is
// the "Time" TaskGraph node every other module node declares a dependency
// on, existing mainly so the dependency edge is explicit and so any future
// module needing "what tick/calendar time is it right now" reads from one
// place rather than each module recomputing calendar math independently.
// ---------------------------------------------------------------------------

namespace dt::sim
{
    // How many fixed ticks make up one in-game minute. At kFixedTickSeconds
    // = 0.016s (~62.5 ticks/sec real-time) and kTicksPerGameMinute = 25,
    // one in-game minute passes every 0.4 real-time seconds at x1 scale -
    // i.e. a full 24 in-game hours takes 1440 * 0.4s = 576s (~9.6 minutes)
    // real-time at x1. This ratio is content tuning, not an engine
    // constant - exposed here as the single place it's defined so Domaintic
    // can retune pacing without touching engine code, but it is NOT wired
    // through Config yet (Config-driven
    // tuning is a follow-up, not assumed done here).
    inline constexpr u64 kTicksPerGameMinute = 25;
    inline constexpr u64 kGameMinutesPerHour = 60;
    inline constexpr u64 kGameHoursPerDay = 24;

    struct CalendarTime
    {
        u64 totalGameMinutes = 0;

        u64 Minute() const { return totalGameMinutes % kGameMinutesPerHour; }
        u64 Hour() const { return (totalGameMinutes / kGameMinutesPerHour) % kGameHoursPerDay; }
        u64 Day() const { return totalGameMinutes / (kGameMinutesPerHour * kGameHoursPerDay); }

        // 0 = Sunday, matching event_create_v0/v1's days_of_week convention
        // used elsewhere in this project's tool integrations, for
        // consistency if Domaintic ever surfaces calendar data through
        // those tools.
        u64 DayOfWeek() const { return Day() % 7; }
    };

    class SimClock
    {
    public:
        // Called once per tick by the "Time" TaskGraph node, before any
        // other node runs (every other pipeline node depends on this one -
        // see SimulationWorld::BuildTickGraph). Pure function of
        // (tickIndex) - no wall-clock reads, matching SimulationLoop's
        // determinism contract.
        void Advance(u64 tickIndex)
        {
            m_tickIndex = tickIndex;
            m_calendar.totalGameMinutes = tickIndex / kTicksPerGameMinute;
        }

        u64 TickIndex() const { return m_tickIndex; }
        const CalendarTime& Calendar() const { return m_calendar; }

    private:
        u64 m_tickIndex = 0;
        CalendarTime m_calendar;
    };
}
