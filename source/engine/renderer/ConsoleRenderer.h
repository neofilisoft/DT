#pragma once

#include "renderer/IRenderer.h"

// ---------------------------------------------------------------------------
// ConsoleRenderer.h
//
// Milestone-1 stand-in for a real GPU backend (VulkanRenderer, planned for
// M2 alongside an SDL3-owned window). Prints a compact one-line-per-frame
// summary of the snapshot instead of drawing anything - its purpose is to
// prove the Application/SimulationLoop/RenderLoop/TripleBufferedSnapshot
// plumbing is correct (tick rate, frame rate, snapshot data arriving
// intact on the render thread) *before* any GPU/windowing complexity is
// introduced. IRenderer is the only header this depends on: no engine
// module outside runtime/renderer is touched.
// ---------------------------------------------------------------------------

namespace dt
{
    class ConsoleRenderer final : public IRenderer
    {
    public:
        bool Initialize() override;
        void Shutdown() override;
        void Render(const SimSnapshot& snapshot) override;
        f32 TargetFramesPerSecond() const override { return 30.0f; } // console I/O is slow; no point exceeding this

    private:
        u64 m_framesRendered = 0;
    };
}
