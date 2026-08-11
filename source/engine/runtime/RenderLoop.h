#pragma once

#include "core/logging/Logger.h"
#include "core/platform/Types.h"
#include "renderer/IRenderer.h"
#include "runtime/SimulationSnapshot.h"

#include <atomic>
#include <thread>

// ---------------------------------------------------------------------------
// RenderLoop.h
//
// Owns the Render thread. Paces itself to IRenderer::TargetFramesPerSecond
// independent of the Simulation thread's tick rate - this is deliberate:
// the render thread may draw the same snapshot more than once (simulation
// running slower than render, or paused) or skip snapshots (simulation
// running far faster than render, e.g. time-scale x100 with a 60 FPS
// display cap) and this is completely fine, because SimSnapshot is a
// self-contained, already-complete projection - there is no "partial
// frame" concept to worry about, unlike coupling directly to sim ticks.
//
// RenderLoop's only dependency into Simulation-side code is
// TripleBufferedSnapshot::AcquireRead (a plain copy, see
// SimulationSnapshot.h) - it never touches SimulationLoop, JobSystem, or
// any simulation module directly, keeping the "Renderer may not modify
// Simulation state" rule enforced at the dependency-graph level, not just
// by convention.
// ---------------------------------------------------------------------------

namespace dt
{
    class RenderLoop
    {
    public:
        RenderLoop(const TripleBufferedSnapshot& snapshot, IRenderer& renderer)
            : m_snapshot(snapshot)
            , m_renderer(renderer)
        {
        }

        void Start()
        {
            DT_ASSERT(!m_thread.joinable(), "RenderLoop::Start called while already running");
            m_running.store(true, std::memory_order_relaxed);
            m_thread = std::thread(&RenderLoop::Run, this);
        }

        void Stop()
        {
            m_running.store(false, std::memory_order_relaxed);
            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }

        f32 MeasuredFramesPerSecond() const { return m_measuredFps.load(std::memory_order_relaxed); }

    private:
        void Run()
        {
            if (!m_renderer.Initialize())
            {
                DT_LOG_ERROR(LogCategory::Renderer, "RenderLoop: renderer Initialize() failed, aborting render thread");
                return;
            }

            const f32 targetFps = m_renderer.TargetFramesPerSecond();
            const auto targetFrameDuration = std::chrono::duration<f64>(1.0 / static_cast<f64>(targetFps));

            SimSnapshot localSnapshot;
            auto frameAnchor = std::chrono::steady_clock::now();

            auto measureWindowStart = frameAnchor;
            u64 framesAtWindowStart = 0;
            u64 frameCount = 0;

            while (m_running.load(std::memory_order_relaxed))
            {
                m_snapshot.AcquireRead(localSnapshot);
                m_renderer.Render(localSnapshot);
                ++frameCount;

                frameAnchor += std::chrono::duration_cast<std::chrono::steady_clock::duration>(targetFrameDuration);
                const auto now = std::chrono::steady_clock::now();
                if (frameAnchor > now)
                {
                    std::this_thread::sleep_until(frameAnchor);
                }
                else
                {
                    frameAnchor = now;
                }

                const auto elapsedInWindow = std::chrono::duration<f64>(now - measureWindowStart).count();
                if (elapsedInWindow >= 0.5)
                {
                    const u64 framesInWindow = frameCount - framesAtWindowStart;
                    m_measuredFps.store(static_cast<f32>(static_cast<f64>(framesInWindow) / elapsedInWindow), std::memory_order_relaxed);
                    measureWindowStart = now;
                    framesAtWindowStart = frameCount;
                }
            }

            m_renderer.Shutdown();
        }

        const TripleBufferedSnapshot& m_snapshot;
        IRenderer& m_renderer;
        std::thread m_thread;
        std::atomic<bool> m_running{ false };
        std::atomic<f32> m_measuredFps{ 0.0f };
    };
}
