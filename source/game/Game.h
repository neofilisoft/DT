#pragma once

#include "core/platform/Types.h"
#include "runtime/SimulationLoop.h"
#include "simulation/world/SimulationWorld.h"

// ---------------------------------------------------------------------------
// Game.h
//
// Deliberately in source/game/, not source/engine/runtime/ or
// source/engine/simulation/ - see this file's original M1 rationale
// (still accurate): DTEngine must have zero Domaintic-specific
// dependencies inside engine/, so anything that knows what a "Sim" or
// "Household" actually is belongs here.
//
// M3 UPDATE: Game now wraps dt::sim::SimulationWorld (the real TaskGraph-
// driven module pipeline: Time -> Needs -> Autonomy -> InteractionResolve
// -> Snapshot, see simulation/world/SimulationWorld.h) instead of M1's
// ToySim placeholder. ToySim's orbit-walking demo entities are gone - they
// were explicitly documented as throwaway M1 content pending "the real
// per-module component set", which now exists. Game's own role shrinks
// accordingly: it no longer implements any tick logic itself, it only
// owns a SimulationWorld instance and exposes its tick function, since
// SimulationWorld is already engine-generic (it doesn't know what
// Domaintic is either - it just runs whatever modules exist). Once
// Domaintic has actual game-specific content (household composition,
// starting lot layout, Domaintic-specific interactions beyond the engine's
// built-in "Rest"/"GrabASnack" global pool), that content-loading logic is
// what will live here, keeping the Domaintic/engine boundary intact.
// ---------------------------------------------------------------------------

namespace dt::game
{
    class Game
    {
    public:
        explicit Game(usize initialEntityCount = 12);

        SimTickFunc MakeTickFunc();

        usize LiveEntityCount() const { return m_world.LiveEntityCount(); }
        const sim::SimulationWorld& World() const { return m_world; }
        sim::SimulationWorld& World_Mutable() { return m_world; }

    private:
        sim::SimulationWorld m_world;
    };
}
