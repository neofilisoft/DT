#pragma once

#include "core/logging/Logger.h"
#include "core/platform/Types.h"
#include "runtime/SimulationSnapshot.h"

#include <atomic>
#include <functional>
#include <thread>

// ---------------------------------------------------------------------------
// SimulationLoop.h
//
// Owns the Simulation thread. Runs a fixed-timestep loop (default 16ms =
// ~62.5 ticks/sec, matching the "60 Tick/s" figure from the architecture
// discussion) independent of wall-clock frame rate and independent of the
// Render thread entirely - SimulationLoop never touches a renderer, never
// includes a renderer header, and has no concept of "frame". Its only
// output is publishing a SimSnapshot via TripleBufferedSnapshot once per
// tick.
//
// TIME SCALE: kFixedTickSeconds (sim-time per tick) is constant - what
// changes with time-scale is how many *wall-clock* seconds the loop is
// willing to sleep between ticks, not the tick's simulated duration. This
// is what keeps determinism: UpdateFixed(dt) is always called with the
// exact same dt regardless of time-scale, so a run at x1 and a run at x100
// produce bit-identical simulation state at the same tickIndex - x100 just
// gets there in 1/100th the wall-clock time by not sleeping between ticks
// (and, once the sim becomes the bottleneck at very high scale, by simply
// running ticks back-to-back as fast as the CPU allows, which is exactly
// the "headless simulation, no real-time pacing at all" mode needed for
// fast-forward / offline batch simulation).
//
// DETERMINISM CONTRACT: the callback passed to Run() must be a pure
// function of (current simulation state, fixed dt) - see JobSystem.h's
// TaskGraph determinism notes for how the per-tick module pipeline
// (Time -> Needs -> Relationship -> AI -> Job Queue -> Navigation ->
// Animation State -> Object State) is expected to be wired as a TaskGraph
// inside that callback in later milestones. SimulationLoop itself does not
// care what's inside the callback; it only guarantees fixed dt and tick
// ordering.
// ---------------------------------------------------------------------------

namespace dt
{
    // Sim-time seconds per tick. 62.5 ticks/sec keeps a round 16ms step;
    // exposed as a named constant (not a magic number at call sites)
    // because save-file replay and any future networked-lockstep code must
    // agree on this exact value.
    inline constexpr f64 kFixedTickSeconds = 0.016;

    using SimTickFunc = std::function<void(u64 tickIndex, f64 fixedDeltaSeconds, SimSnapshot& outSnapshot)>;

    class SimulationLoop
    {
    public:
        SimulationLoop(TripleBufferedSnapshot& snapshot, SimTickFunc tickFunc)
            : m_snapshot(snapshot)
            , m_tickFunc(std::move(tickFunc))
        {
        }

        // Starts the simulation thread. Non-blocking - returns immediately,
        // the loop runs on its own std::thread until Stop() is called.
        void Start()
        {
            DT_ASSERT(!m_thread.joinable(), "SimulationLoop::Start called while already running");
            m_running.store(true, std::memory_order_relaxed);
            m_thread = std::thread(&SimulationLoop::Run, this);
        }

        void Stop()
        {
            m_running.store(false, std::memory_order_relaxed);
            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }

        // Thread-safe. 1.0 = real-time, 8.0 = 8x, 100.0 = 100x, 0.0 = paused.
        // Read by the loop once per tick, so a change takes effect within
        // one tick, never mid-tick.
        void SetTimeScale(f32 scale)
        {
            DT_ASSERT(scale >= 0.0f, "SimulationLoop::SetTimeScale: negative time scale is not valid");
            m_timeScale.store(scale, std::memory_order_relaxed);
        }

        f32 GetTimeScale() const { return m_timeScale.load(std::memory_order_relaxed); }

        u64 TickCount() const { return m_tickIndex.load(std::memory_order_relaxed); }

        // Rolling measurement of achieved ticks/sec over the last
        // measurement window, for the RenderLoop's debug HUD. Updated by
        // the sim thread, read by the render thread - a plain relaxed
        // atomic<f32> is sufficient since it is display-only and has no
        // bearing on simulation correctness.
        f32 MeasuredTicksPerSecond() const { return m_measuredTps.load(std::memory_order_relaxed); }

    private:
        void Run()
        {
            DT_LOG_INFO(LogCategory::Simulation, "SimulationLoop starting (fixed tick = {:.4f}s / {:.1f} ticks/sec at x1)",
                kFixedTickSeconds, 1.0 / kFixedTickSeconds);

            f64 simTimeSeconds = 0.0;
            auto wallClockAnchor = std::chrono::steady_clock::now();

            // Windowed tick-rate measurement.
            auto measureWindowStart = wallClockAnchor;
            u64 ticksAtWindowStart = 0;

            while (m_running.load(std::memory_order_relaxed))
            {
                const f32 scale = m_timeScale.load(std::memory_order_relaxed);

                if (scale <= 0.0f)
                {
                    // Paused: don't advance sim time, don't burn CPU.
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    wallClockAnchor = std::chrono::steady_clock::now();
                    continue;
                }

                const u64 tickIndex = m_tickIndex.fetch_add(1, std::memory_order_relaxed);
                simTimeSeconds += kFixedTickSeconds;

                SimSnapshot& target = m_snapshot.BeginWrite();
                m_tickFunc(tickIndex, kFixedTickSeconds, target);
                target.tickIndex = tickIndex;
                target.simTimeSeconds = simTimeSeconds;
                target.timeScale = scale;
                m_snapshot.CommitWrite();

                // Pace to wall-clock only when scale keeps us below "run as
                // fast as possible". At scale=1, sleep enough that ticks
                // land ~kFixedTickSeconds apart in real time. At high scale
                // (x100), the required wall-clock-per-tick shrinks to
                // 0.16ms, which in practice means "don't sleep, just keep
                // ticking" - the loop naturally becomes CPU-bound headless
                // simulation, which is exactly the desired x100 behavior.
                const auto wallSecondsPerTick = std::chrono::duration<f64>(kFixedTickSeconds / static_cast<f64>(scale));
                wallClockAnchor += std::chrono::duration_cast<std::chrono::steady_clock::duration>(wallSecondsPerTick);
                const auto now = std::chrono::steady_clock::now();
                if (wallClockAnchor > now)
                {
                    std::this_thread::sleep_until(wallClockAnchor);
                }
                else
                {
                    // Fell behind (or scale is high enough that pacing is
                    // effectively disabled) - resync the anchor to now
                    // rather than accumulating an ever-growing backlog of
                    // "catch-up" ticks, which would violate fixed-timestep
                    // determinism expectations (we do NOT do the
                    // variable-substep catch-up some real-time engines do;
                    // headless/offline batch simulation is expected to use
                    // a dedicated non-realtime-paced run mode instead, not
                    // this catch-up path).
                    wallClockAnchor = now;
                }

                const auto elapsedInWindow = std::chrono::duration<f64>(now - measureWindowStart).count();
                if (elapsedInWindow >= 0.5)
                {
                    const u64 ticksInWindow = tickIndex - ticksAtWindowStart;
                    m_measuredTps.store(static_cast<f32>(static_cast<f64>(ticksInWindow) / elapsedInWindow), std::memory_order_relaxed);
                    measureWindowStart = now;
                    ticksAtWindowStart = tickIndex;
                }
            }

            DT_LOG_INFO(LogCategory::Simulation, "SimulationLoop stopped at tick {}", m_tickIndex.load(std::memory_order_relaxed));
        }

        TripleBufferedSnapshot& m_snapshot;
        SimTickFunc m_tickFunc;
        std::thread m_thread;
        std::atomic<bool> m_running{ false };
        std::atomic<f32> m_timeScale{ 1.0f };
        std::atomic<u64> m_tickIndex{ 0 };
        std::atomic<f32> m_measuredTps{ 0.0f };
    };
}
