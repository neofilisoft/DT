#pragma once

#include "core/logging/Logger.h"
#include "renderer/IRenderer.h"
#include "runtime/RenderLoop.h"
#include "runtime/SimulationLoop.h"
#include "runtime/SimulationSnapshot.h"

#include <atomic>
#include <csignal>

// ---------------------------------------------------------------------------
// Application.h
//
//                Application
//                     |
//     +---------------+---------------+
//     |                               |
// Simulation Thread              Render Thread
//     |                               |
//     |                          (IRenderer)
//     |                               |
//     +---------- Snapshot -----------+
//
// This class is the concrete realization of the diagram from the
// architecture discussion. It owns exactly three things: the
// TripleBufferedSnapshot (the only channel between the two threads), a
// SimulationLoop, and a RenderLoop - and it owns them in that order so
// that the snapshot outlives both loops (both loops hold references to
// it, never ownership).
//
// Application does not know what "the game" does. The sim tick callback
// (what actually runs Time -> Needs -> Relationship -> AI -> ... per the
// module list) is supplied by the caller (source/game), keeping engine/
// entirely game-content-agnostic - DTEngine is the reusable substrate,
// source/game is Domaintic-specific.
// ---------------------------------------------------------------------------

namespace dt
{
    class Application
    {
    public:
        Application(SimTickFunc tickFunc, IRenderer& renderer)
            : m_simLoop(m_snapshot, std::move(tickFunc))
            , m_renderLoop(m_snapshot, renderer)
        {
        }

        // Starts both threads and blocks the calling thread, polling for
        // shutdown (Ctrl+C in this M1 console harness; a real platform
        // layer's window-close/WM_QUIT would replace this poll in M2).
        // Deliberately does NOT put any simulation or render work on the
        // calling thread - the calling thread here is purely a lifetime
        // owner/supervisor, matching "Main Loop" in the architecture
        // diagram being a thin driver, not where PollEvents/UpdateFixed/
        // Render actually execute (those live on their respective owned
        // threads in this design; a future SDL3 PollEvents call belongs
        // on the render/platform thread since window events are
        // inherently tied to the OS window, not to simulation).
        void Run()
        {
            DT_LOG_INFO(LogCategory::Core, "Application starting: DTEngine / Domaintic (M1 harness)");

            s_shutdownRequested.store(false, std::memory_order_relaxed);
            std::signal(SIGINT, &Application::OnSignal);

            m_simLoop.Start();
            m_renderLoop.Start();

            while (!s_shutdownRequested.load(std::memory_order_relaxed))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            DT_LOG_INFO(LogCategory::Core, "Application shutdown requested, stopping threads...");
            m_simLoop.Stop();
            m_renderLoop.Stop();
            DT_LOG_INFO(LogCategory::Core, "Application stopped cleanly.");
        }

        void RequestShutdown() { s_shutdownRequested.store(true, std::memory_order_relaxed); }

        SimulationLoop& Sim() { return m_simLoop; }
        RenderLoop& Render() { return m_renderLoop; }

    private:
        static void OnSignal(int) { s_shutdownRequested.store(true, std::memory_order_relaxed); }

        TripleBufferedSnapshot m_snapshot;
        SimulationLoop m_simLoop;
        RenderLoop m_renderLoop;

        static inline std::atomic<bool> s_shutdownRequested{ false };
    };
}
