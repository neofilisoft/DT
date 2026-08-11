#pragma once

#include "runtime/SimulationSnapshot.h"

// ---------------------------------------------------------------------------
// IRenderer.h
//
// The entire contract a renderer backend must satisfy. Notice what is NOT
// here: no access to JobSystem, no access to Simulation module internals,
// no Handle<T>-based lookups into ComponentArray<T>. A renderer backend
// (this milestone's ConsoleRenderer; a future VulkanRenderer) receives
// only a SimSnapshot - a flat, POD-only, already-computed projection of
// what to draw - and nothing else. This is the concrete mechanism behind
// "Renderer must not modify Simulation state": a VulkanRenderer literally
// has no reference to simulation state to modify, because the interface
// never hands it one. Swapping ConsoleRenderer for VulkanRenderer later
// requires zero changes to SimulationLoop, RenderLoop's control flow, or
// Application - only a different IRenderer implementation gets
// constructed at startup.
// ---------------------------------------------------------------------------

namespace dt
{
    class IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        // Backend setup (window/swapchain/device creation for a real
        // backend; a no-op for ConsoleRenderer). Called once on the render
        // thread before the first Render() call - backend resource
        // creation happens on the thread that will use those resources.
        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;

        // Called once per render frame with the latest available
        // snapshot. Implementations must treat `snapshot` as read-only.
        virtual void Render(const SimSnapshot& snapshot) = 0;

        // Render-thread pacing target in frames/sec, independent of the
        // simulation's tick rate (see SimulationLoop.h) - this is exactly
        // the "Renderer 120 FPS while Simulation runs at 480 Tick/s at
        // x8" split from the architecture discussion.
        virtual f32 TargetFramesPerSecond() const { return 60.0f; }
    };
}
