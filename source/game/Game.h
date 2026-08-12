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
// Game wraps dt::sim::SimulationWorld (the TaskGraph-driven module pipeline:
// Time -> Needs -> Autonomy -> InteractionResolve -> Snapshot,
// see simulation/world/SimulationWorld.h). Game's role is to own a
// SimulationWorld instance and expose its tick function, since
// SimulationWorld is already engine-generic. Once
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
