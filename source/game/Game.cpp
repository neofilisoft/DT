#include "Game.h"

// ---------------------------------------------------------------------------
// Game.cpp
//
// Thin wrapper delegating entirely to SimulationWorld. See Game.h for
// why the M1 ToySim implementation was removed rather than kept alongside.
// ---------------------------------------------------------------------------

namespace dt::game
{
    Game::Game(usize initialEntityCount)
        : m_world(initialEntityCount)
    {
    }

    SimTickFunc Game::MakeTickFunc()
    {
        return m_world.MakeTickFunc();
    }
}
