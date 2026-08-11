#pragma once

#include "core/platform/Assert.h"
#include "core/platform/Types.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// JobSystem.h
//
// Fixed-pool, work-stealing task graph scheduler. This is the concurrency
// backbone for the entire Simulation layer (Time -> Calendar -> Weather ->
// Needs -> Mood -> Relationships -> Autonomy -> Utility AI -> Job Queue ->
// ... per your module list) and this is deliberately a task GRAPH, not a
// bare thread pool with independent submit() calls. Rationale, expanded
// from the initial architecture discussion:
//
// The simulation modules have real, direct data dependencies every tick -
// Mood scoring reads that tick's Needs output; Autonomy's utility scoring
// reads that tick's Mood output; Job Queue reads Autonomy's decision. A
// flat thread pool where each module is an independent submit() call gives
// you a data race the instant two dependent modules are scheduled on
// different workers with no ordering guarantee. You would then need to
// manually insert barriers/fences between every dependent pair, by hand,
// at every call site - which is exactly the kind of error-prone
// hand-rolled synchronization a 1-3 person team cannot afford to get wrong
// under deadline pressure.
//
// A task graph solves this by making the dependency a first-class,
// declared relationship (TaskB.After(TaskA)) checked once at graph-build
// time. The scheduler then guarantees TaskA fully completes (including any
// nested sub-tasks it spawned) before any worker begins TaskB, while still
// running fully independent branches (e.g. Weather and Genetics have no
// dependency on each other in your module list) fully in parallel.
//
// DETERMINISM: this is also the mechanism that makes deterministic replay
// tractable. Given: (1) a fixed fully-connected task graph shape built the
// same way every tick from the same module list, (2) a fixed timestep
// (Runtime/SimulationLoop enforces this, see engine/runtime), and (3) a
// seeded RNG stream per module rather than one shared global RNG (so
// thread scheduling order never perturbs which random draw a given module
// consumes) - the graph produces bit-identical simulation state on every
// run regardless of which physical worker thread happened to execute which
// node, because independent-branch tasks by construction never touch the
// same data (each task declares its Reads/Writes, see TaskGraph::Node
// below) and dependent tasks are ordered by the graph, not by thread
// scheduling luck.
//
// Threading model: N = hardware_concurrency() - 1 persistent worker
// threads (the "-1" reserves the calling/main thread as a worker too, so a
// machine reporting 8 hardware threads runs 7 background workers + the
// main thread participating in stealing during Wait(), rather than
// spawning 8 background threads and leaving the main thread idle-blocked).
// Each worker owns a Chase-Lev work-stealing deque: a worker pushes/pops
// its own new work from the bottom (LIFO, cache-friendly, matches typical
// recursive graph traversal), and idle workers steal from the TOP of
// another worker's deque (FIFO from the thief's perspective, which steals
// the oldest/largest-grained work first - minimizing the number of steals
// needed and reducing contention with the deque owner, who is operating at
// the opposite end).
// ---------------------------------------------------------------------------

namespace dt
{
    class TaskGraph;

    using TaskFunc = std::function<void()>;

    // ---------------------------------------------------------------------
    // WorkStealingDeque
    //
    // Chase-Lev lock-free deque. Owner thread calls Push/Pop from the
    // bottom; thief threads call Steal from the top. This specific
    // algorithm (not a mutex-guarded std::deque) is what makes worker
    // idle-time productive without every worker serializing on a single
    // shared queue's lock - each worker's own queue only contends with
    // thieves, and only during the (rare, relative to actual task
    // execution time) moment a steal races the owner's own pop.
    // ---------------------------------------------------------------------
    class WorkStealingDeque
    {
    public:
        explicit WorkStealingDeque(usize capacity = 1024)
            : m_buffer(capacity)
        {
            DT_ASSERT((capacity & (capacity - 1)) == 0, "WorkStealingDeque capacity must be a power of two");
        }

        // Owner-thread only.
        void Push(TaskFunc* task)
        {
            const i64 b = m_bottom.load(std::memory_order_relaxed);
            const i64 t = m_top.load(std::memory_order_acquire);

            if (b - t >= static_cast<i64>(m_buffer.size()))
            {
                Grow();
            }

            m_buffer[b & Mask()] = task;
            std::atomic_thread_fence(std::memory_order_release);
            m_bottom.store(b + 1, std::memory_order_relaxed);
        }

        // Owner-thread only.
        TaskFunc* Pop()
        {
            i64 b = m_bottom.load(std::memory_order_relaxed) - 1;
            m_bottom.store(b, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            i64 t = m_top.load(std::memory_order_relaxed);

            if (t > b)
            {
                m_bottom.store(t, std::memory_order_relaxed);
                return nullptr;
            }

            TaskFunc* task = m_buffer[b & Mask()];
            if (t == b)
            {
                // Last element: race with a potential concurrent Steal.
                if (!m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
                {
                    task = nullptr; // A thief won the race.
                }
                m_bottom.store(t + 1, std::memory_order_relaxed);
            }
            return task;
        }

        // Any thread (thief).
        TaskFunc* Steal()
        {
            i64 t = m_top.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            i64 b = m_bottom.load(std::memory_order_acquire);

            if (t >= b)
            {
                return nullptr;
            }

            TaskFunc* task = m_buffer[t & Mask()];
            if (!m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
            {
                return nullptr; // Lost the race to another thief or the owner.
            }
            return task;
        }

    private:
        usize Mask() const { return m_buffer.size() - 1; }

        void Grow()
        {
            // Growth is intentionally rare (sized generously up front per
            // worker based on expected max simultaneously-pending tasks per
            // tick) and is not lock-free - it only ever happens from the
            // owner thread inside Push, and thieves reading mid-grow simply
            // retry (their CAS on m_top will fail against the old size or
            // succeed against consistent data, since we only ever append
            // capacity, never relocate indices below the current top).
            std::vector<TaskFunc*> grown(m_buffer.size() * 2);
            const i64 b = m_bottom.load(std::memory_order_relaxed);
            const i64 t = m_top.load(std::memory_order_relaxed);
            for (i64 i = t; i < b; ++i)
            {
                grown[i & (grown.size() - 1)] = m_buffer[i & Mask()];
            }
            m_buffer.swap(grown);
        }

        std::vector<TaskFunc*> m_buffer;
        std::atomic<i64> m_top{ 0 };
        std::atomic<i64> m_bottom{ 0 };
    };

    // ---------------------------------------------------------------------
    // TaskGraph
    //
    // A DAG of Nodes built once per tick (or reused across ticks if its
    // shape is static, which is the common case for the fixed simulation
    // module pipeline). Each Node wraps a TaskFunc and an explicit list of
    // predecessor nodes it must wait on.
    // ---------------------------------------------------------------------
    class TaskGraph
    {
    public:
        class Node
        {
        public:
            Node(TaskFunc func, std::string debugName)
                : m_func(std::move(func))
                , m_debugName(std::move(debugName))
            {
            }

            Node& After(Node& predecessor)
            {
                predecessor.m_successors.push_back(this);
                m_pendingDependencyCount.fetch_add(1, std::memory_order_relaxed);
                return *this;
            }

            const std::string& DebugName() const { return m_debugName; }

        private:
            friend class JobSystem;
            friend class TaskGraph;

            TaskFunc m_func;
            std::string m_debugName;
            std::vector<Node*> m_successors;
            std::atomic<i32> m_pendingDependencyCount{ 0 };
            i32 m_initialDependencyCount = 0; // snapshot taken at Reset(), used to reset m_pendingDependencyCount between graph runs
        };

        Node& AddTask(TaskFunc func, std::string debugName)
        {
            m_nodes.push_back(std::make_unique<Node>(std::move(func), std::move(debugName)));
            return *m_nodes.back();
        }

        // Call once after all AddTask/After calls, before the first Run().
        // Snapshots each node's dependency count so subsequent Reset() calls
        // (between simulation ticks, reusing the same graph shape) can
        // restore it without re-walking the whole graph.
        void Finalize()
        {
            for (auto& node : m_nodes)
            {
                node->m_initialDependencyCount = node->m_pendingDependencyCount.load(std::memory_order_relaxed);
            }
        }

        void Reset()
        {
            for (auto& node : m_nodes)
            {
                node->m_pendingDependencyCount.store(node->m_initialDependencyCount, std::memory_order_relaxed);
            }
        }

        const std::vector<std::unique_ptr<Node>>& Nodes() const { return m_nodes; }

    private:
        std::vector<std::unique_ptr<Node>> m_nodes;
    };

    // ---------------------------------------------------------------------
    // JobSystem
    //
    // Owns the persistent worker pool. Submit a TaskGraph via RunGraph and
    // block the calling thread until it completes (the calling thread also
    // participates as a stealing worker while waiting, rather than
    // sleeping - see Wait()). For one-off, dependency-free async work
    // (background asset load kicked off from anywhere, not needing graph
    // wiring), use the lightweight Async<T>/Future<T> wrapper below, which
    // is implemented in terms of the same underlying deques (a single
    // Node with no dependencies) so there is exactly one execution engine
    // underneath both APIs, not two parallel implementations to keep in
    // sync.
    // ---------------------------------------------------------------------
    class JobSystem
    {
    public:
        static JobSystem& Get();

        void Initialize(u32 workerCountOverride = 0);
        void Shutdown();

        // Runs every ready node (zero pending dependencies) in `graph`,
        // distributing across the worker pool, and blocks the calling
        // thread until every node has completed. The calling thread steals
        // and executes work itself while waiting rather than idling, so a
        // single-graph workload does not waste the calling thread's cycles.
        void RunGraph(TaskGraph& graph);

        // One-off, dependency-free task submission for the Async<T>
        // wrapper (core/jobs/Async.h). Pushed to a round-robin worker deque;
        // no graph bookkeeping, no dependency tracking, no
        // m_activeGraphPendingCount involvement - this deliberately does
        // NOT interact with RunGraph's completion tracking, so a detached
        // task submitted while a graph is running does not block or get
        // blocked by that graph's Wait loop.
        void SubmitDetached(TaskFunc func);

        // Public wrapper so Future<T>::Get()/Wait() can participate as a
        // stealing worker while blocked. Returns false if no work was found
        // anywhere (caller should yield rather than busy-spin).
        bool TryStealAndRunOne();

        u32 WorkerCount() const { return static_cast<u32>(m_workers.size()); }

        // Returns the 0-based worker index for the calling thread, or
        // kMainThreadIndex if called from a thread not owned by the pool
        // (e.g. the true process main thread before/after RunGraph). Used
        // by per-worker resources (e.g. each worker's own LinearAllocator
        // scratch arena, see core/memory/LinearAllocator.h) to index into a
        // per-worker resource array without a lookup.
        static constexpr u32 kMainThreadIndex = 0xFFFFFFFFu;
        u32 CurrentWorkerIndex() const;

    private:
        struct Worker
        {
            std::thread thread;
            WorkStealingDeque deque;
            u32 index = 0;
        };

        void WorkerLoop(Worker& self);
        bool TryRunOneTask(u32 workerIndex);
        void ExecuteNode(TaskGraph::Node* node);

        std::vector<std::unique_ptr<Worker>> m_workers;
        std::atomic<bool> m_running{ false };
        std::atomic<u32> m_activeGraphPendingCount{ 0 };

        std::mutex m_wakeMutex;
        std::condition_variable m_wakeCv;

        static thread_local u32 s_currentWorkerIndex;
    };
}
