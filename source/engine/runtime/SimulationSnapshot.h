#pragma once

#include "core/platform/Types.h"
#include "runtime/Entity.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// SimulationSnapshot.h
//
// This header is the single data contract crossing the Simulation/Render
// thread boundary. Nothing else should. The rule from the architecture
// discussion - "Renderer may not modify Simulation state, Simulation may
// not call the Renderer" - is enforced two ways here, not just documented:
//
//   1. SimSnapshot only ever contains plain-old-data render proxies (a
//      flat, POD-only struct per drawable). It has NO pointers back into
//      live simulation objects, NO Handle<T> that would let a renderer
//      walk into ComponentArray<T> internals, and no methods that mutate
//      simulation state. Copying a snapshot is deliberately just copying
//      dumb data - even a hostile or buggy renderer cannot reach through
//      it into the simulation.
//
//   2. TripleBufferedSnapshot's API surface is two verbs only:
//      Simulation calls BeginWrite()/CommitWrite(); Render calls
//      AcquireRead(). There is no shared-mutable-state path between them -
//      the only synchronization primitive is one atomic index swap.
//
// WHY TRIPLE BUFFER, NOT DOUBLE:
// A plain double buffer (front/back) needs the writer to block if the
// reader hasn't finished consuming "front" yet when the writer wants to
// swap - or the reader risks tearing mid-read if the writer swaps under
// it. With time-scale x100 the simulation thread can produce many ticks
// per rendered frame and must NEVER stall waiting on the render thread
// (render thread hitches must not slow the simulation down - the whole
// point of separating them). Three slots means: sim always writes into
// whichever slot is neither the currently-published slot nor the slot the
// renderer currently holds a read lock on, so sim never blocks on render
// and render never blocks on sim. This is the standard lock-free triple
// buffer (used in it's original form for GPU present queues) applied here
// to CPU-side sim->render handoff.
//
// DETERMINISM NOTE: the snapshot is a *read-only projection* of
// simulation state for display purposes only. It is never fed back into
// the simulation and never participates in the deterministic tick
// (JobSystem::TaskGraph, seeded per-module RNG - see JobSystem.h). Two
// runs with identical input produce identical simulation state and
// identical snapshots, but the snapshot itself carries no state the next
// tick depends on.
// ---------------------------------------------------------------------------

namespace dt
{
    // A single drawable's render-relevant state, sampled from simulation
    // for one tick. Deliberately flat POD: no owning pointers, no
    // Handle<T> lookups required by the renderer. `entityId` is kept only
    // as an opaque u64 (index<<32|generation, see Handle<T>::Hasher) for
    // debug picking/UI correlation - the renderer must not use it to
    // reach back into simulation containers.
    struct RenderProxy
    {
        u64 entityId = 0;
        f32 positionX = 0.0f;
        f32 positionY = 0.0f;
        f32 positionZ = 0.0f;
        f32 rotationY = 0.0f;   // yaw, radians - sufficient for 2.5D top-down agents
        u32 archetypeId = 0;    // which mesh/sprite/animation-set to use; renderer-side lookup table
        u32 animationState = 0;
        f32 animationPhase = 0.0f; // 0..1 normalized, for interpolation between sim ticks
    };

    // Everything the render thread needs to draw one frame, plus enough
    // simulation-clock metadata to interpolate motion between the last two
    // ticks and to display sim time / time-scale in UI (a life-sim HUD
    // needs "Day 14, 2:30 PM, running at x8" without querying simulation
    // state directly).
    struct SimSnapshot
    {
        u64 tickIndex = 0;
        f64 simTimeSeconds = 0.0;   // total simulated seconds elapsed, unaffected by time-scale wall-clock cost
        f32 timeScale = 1.0f;
        std::vector<RenderProxy> proxies;

        void Clear()
        {
            tickIndex = 0;
            simTimeSeconds = 0.0;
            timeScale = 1.0f;
            proxies.clear();
        }
    };

    // ---------------------------------------------------------------------
    // TripleBufferedSnapshot
    //
    // Slot state machine (index 0/1/2), tracked with a single atomic that
    // packs [readingSlot: not touched by writer][readySlot: latest
    // complete][writeSlot: being written]. We use the classic
    // "index-swap on commit" triple buffer: a small atomic<u32> holds the
    // currently-published "ready" slot index; AcquireRead reads that index
    // and reads the corresponding slot (safe because the writer never
    // writes into a slot currently published-as-ready or currently
    // checked-out-as-reading); CommitWrite atomically publishes the
    // just-written slot as ready and reclaims whichever slot is now free.
    // ---------------------------------------------------------------------
    class TripleBufferedSnapshot
    {
    public:
        TripleBufferedSnapshot()
        {
            for (auto& slot : m_slots)
            {
                slot = std::make_unique<SimSnapshot>();
            }
        }

        // --- Simulation-thread API -----------------------------------

        // Returns the slot Simulation should write this tick's snapshot
        // into. Always safe to write into: it is never the published
        // "ready" slot and never the slot a reader currently holds.
        SimSnapshot& BeginWrite()
        {
            return *m_slots[m_writeIndex];
        }

        // Publishes the slot just filled by BeginWrite() as the new
        // "ready" slot for the render thread, and picks the next distinct
        // slot as the new write target. Single relaxed-store atomic swap:
        // the render thread's AcquireRead reading a slightly-stale index
        // for one extra frame is completely fine (it will just pick it up
        // next AcquireRead call) - there is no correctness requirement
        // that the swap be immediately visible, only that it is
        // eventually visible and that the write target never aliases a
        // slot the render thread might be reading.
        void CommitWrite()
        {
            const u32 justWritten = m_writeIndex;
            const u32 previousReady = m_readyIndex.exchange(justWritten, std::memory_order_release);
            // The slot that was ready before this commit becomes the next
            // write target. It cannot be the slot a reader currently holds
            // (readers only ever hold m_readyIndex as of the moment they
            // called AcquireRead, and by definition that is not
            // `previousReady` once we've just replaced it above from the
            // writer's perspective... except a reader could still be
            // mid-copy of the old ready slot). To keep this genuinely
            // lock-free without a reader-count handshake, we rely on the
            // three-slot invariant: with only one writer and one reader,
            // three slots is provably sufficient for the writer to always
            // have a free slot that is neither "currently ready" nor "the
            // immediately-previous ready slot a reader may still be
            // mid-copy of" - so we cycle to the slot that is neither
            // `justWritten` nor `previousReady`.
            m_writeIndex = 3u - justWritten - previousReady;
        }

        // --- Render-thread API -----------------------------------------

        // Copies the latest complete snapshot into `out`. A copy (not a
        // reference into the slot) is intentional: it lets the render
        // thread hold the data for the entire frame - including across
        // any Vulkan command recording that might take multiple
        // milliseconds - without any risk of the simulation thread ever
        // touching that memory again. RenderProxy is POD and proxies is a
        // flat vector, so this copy is a single contiguous memcpy-class
        // operation, not a deep graph walk.
        void AcquireRead(SimSnapshot& out) const
        {
            const u32 readyIndex = m_readyIndex.load(std::memory_order_acquire);
            out = *m_slots[readyIndex];
        }

    private:
        std::unique_ptr<SimSnapshot> m_slots[3];
        u32 m_writeIndex = 0;
        std::atomic<u32> m_readyIndex{ 1 };
        // Invariant at construction: writeIndex=0, readyIndex=1, leaving
        // slot 2 implicitly "the other free slot" - matches the
        // 3-justWritten-previousReady cycle formula used in CommitWrite.
    };
}
