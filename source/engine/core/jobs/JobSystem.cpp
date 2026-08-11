#include "core/jobs/JobSystem.h"
#include "core/logging/Logger.h"
#include "core/profiler/Profiler.h"

namespace dt
{
    thread_local u32 JobSystem::s_currentWorkerIndex = JobSystem::kMainThreadIndex;

    JobSystem& JobSystem::Get()
    {
        static JobSystem instance;
        return instance;
    }

    void JobSystem::Initialize(u32 workerCountOverride)
    {
        DT_ASSERT(!m_running.load(), "JobSystem::Initialize called twice without Shutdown");

        const u32 hwThreads = std::thread::hardware_concurrency();
        // hardware_concurrency() - 1: the calling/main thread participates
        // as a worker during RunGraph's Wait loop (see RunGraph below), so
        // spawning hardware_concurrency() background threads on top of that
        // would oversubscribe the machine by one. hardware_concurrency() can
        // return 0 on some platforms/VMs if it cannot determine core count;
        // guard against that producing an unsigned wraparound to a huge
        // worker count.
        const u32 defaultWorkers = (hwThreads > 1) ? (hwThreads - 1) : 1;
        const u32 workerCount = (workerCountOverride > 0) ? workerCountOverride : defaultWorkers;

        DT_LOG_INFO(LogCategory::Jobs, "JobSystem initializing with {} worker thread(s) (hardware_concurrency={})",
            workerCount, hwThreads);

        m_running.store(true, std::memory_order_release);

        m_workers.reserve(workerCount);
        for (u32 i = 0; i < workerCount; ++i)
        {
            auto worker = std::make_unique<Worker>();
            worker->index = i;
            m_workers.push_back(std::move(worker));
        }

        // Threads are started in a second pass after all Worker objects
        // exist in m_workers, since WorkerLoop reads m_workers.size() to
        // find steal targets - starting a thread mid-populate could race
        // against the vector still being resized.
        for (auto& worker : m_workers)
        {
            Worker* rawPtr = worker.get();
            worker->thread = std::thread([this, rawPtr]() { WorkerLoop(*rawPtr); });
        }
    }

    void JobSystem::Shutdown()
    {
        m_running.store(false, std::memory_order_release);
        m_wakeCv.notify_all();

        for (auto& worker : m_workers)
        {
            if (worker->thread.joinable())
            {
                worker->thread.join();
            }
        }
        m_workers.clear();

        DT_LOG_INFO(LogCategory::Jobs, "JobSystem shut down");
    }

    u32 JobSystem::CurrentWorkerIndex() const
    {
        return s_currentWorkerIndex;
    }

    void JobSystem::ExecuteNode(TaskGraph::Node* node)
    {
        {
            DT_PROFILE_SCOPE_DYNAMIC(node->DebugName().c_str());
            node->m_func();
        }

        // Decrement every successor's dependency count; any successor that
        // hits zero is now ready and gets pushed onto the current worker's
        // own deque (LIFO). Pushing onto the *executing* worker's deque
        // rather than a shared ready-queue keeps cache locality: the worker
        // that just finished a task's data is likely to still have related
        // cache lines warm for a dependent task.
        //
        // NOTE: m_activeGraphPendingCount is initialized once in RunGraph
        // to graph.Nodes().size() - i.e. it already counts every node in
        // the graph exactly once, regardless of dependency edges. This
        // function must NOT increment it again when a successor becomes
        // ready (that successor was already included in the initial
        // count); only the fetch_sub below, exactly once per node
        // completing, is needed to drain it to zero.
        for (TaskGraph::Node* successor : node->m_successors)
        {
            const i32 remaining = successor->m_pendingDependencyCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (remaining == 0)
            {
                const u32 worker = CurrentWorkerIndex();
                if (worker != kMainThreadIndex && worker < m_workers.size())
                {
                    TaskFunc* taskSlot = new TaskFunc([this, successor]() { ExecuteNode(successor); });
                    m_workers[worker]->deque.Push(taskSlot);
                }
                else
                {
                    // Main thread found a ready successor while waiting on
                    // RunGraph but is not itself a pool worker with a deque
                    // (kMainThreadIndex has no Worker entry) - hand it to
                    // worker 0's deque instead so it still gets picked up by
                    // the stealing pool.
                    if (!m_workers.empty())
                    {
                        TaskFunc* taskSlot = new TaskFunc([this, successor]() { ExecuteNode(successor); });
                        m_workers[0]->deque.Push(taskSlot);
                    }
                }
            }
        }

        m_activeGraphPendingCount.fetch_sub(1, std::memory_order_acq_rel);
        m_wakeCv.notify_all();
    }

    bool JobSystem::TryRunOneTask(u32 workerIndex)
    {
        // First try our own deque (owner-side Pop, LIFO), then attempt to
        // steal from every other worker in round-robin order starting from
        // a different offset per call to avoid all idle workers hammering
        // worker 0 first every time.
        WorkStealingDeque& ownDeque = m_workers[workerIndex]->deque;
        if (TaskFunc* task = ownDeque.Pop())
        {
            (*task)();
            delete task;
            return true;
        }

        const usize workerCount = m_workers.size();
        for (usize offset = 1; offset < workerCount; ++offset)
        {
            const usize victimIndex = (workerIndex + offset) % workerCount;
            if (TaskFunc* stolen = m_workers[victimIndex]->deque.Steal())
            {
                (*stolen)();
                delete stolen;
                return true;
            }
        }

        return false;
    }

    void JobSystem::WorkerLoop(Worker& self)
    {
        s_currentWorkerIndex = self.index;

        while (m_running.load(std::memory_order_acquire))
        {
            if (!TryRunOneTask(self.index))
            {
                // No work found anywhere: wait briefly rather than
                // busy-spinning and burning a full core doing nothing
                // between simulation ticks (the engine's tick rate is
                // fixed-timestep, so there are real idle gaps between
                // graph runs, e.g. waiting on the next fixed-update
                // boundary or on the render thread). A condition variable
                // with a short timeout balances wake latency against not
                // pegging CPU usage at 100% across all cores permanently.
                std::unique_lock<std::mutex> lock(m_wakeMutex);
                m_wakeCv.wait_for(lock, std::chrono::microseconds(200));
            }
        }
    }

    void JobSystem::SubmitDetached(TaskFunc func)
    {
        if (m_workers.empty())
        {
            // No pool initialized (e.g. a tool or unit test running without
            // JobSystem::Initialize): run inline rather than dropping the
            // task, since silently discarding submitted work would be a far
            // worse failure mode than the caller not getting the
            // parallelism it expected.
            func();
            return;
        }

        static std::atomic<u32> s_roundRobin{ 0 };
        const u32 target = s_roundRobin.fetch_add(1, std::memory_order_relaxed) % static_cast<u32>(m_workers.size());

        TaskFunc* taskSlot = new TaskFunc(std::move(func));
        m_workers[target]->deque.Push(taskSlot);
        m_wakeCv.notify_all();
    }

    bool JobSystem::TryStealAndRunOne()
    {
        const u32 caller = CurrentWorkerIndex();
        if (caller != kMainThreadIndex && caller < m_workers.size())
        {
            return TryRunOneTask(caller);
        }

        // Calling thread is not a pool worker (true main thread): steal from
        // every worker deque directly since there is no "own deque" to pop
        // from first.
        for (auto& worker : m_workers)
        {
            if (TaskFunc* stolen = worker->deque.Steal())
            {
                (*stolen)();
                delete stolen;
                return true;
            }
        }
        return false;
    }

    void JobSystem::RunGraph(TaskGraph& graph)
    {
        DT_PROFILE_SCOPE("JobSystem::RunGraph");

        graph.Reset();

        u32 readyCount = 0;
        for (auto& node : graph.Nodes())
        {
            if (node->m_pendingDependencyCount.load(std::memory_order_relaxed) == 0)
            {
                TaskFunc* taskSlot = new TaskFunc([this, ptr = node.get()]() { ExecuteNode(ptr); });
                if (!m_workers.empty())
                {
                    m_workers[readyCount % m_workers.size()]->deque.Push(taskSlot);
                }
                ++readyCount;
            }
        }

        m_activeGraphPendingCount.store(static_cast<u32>(graph.Nodes().size()), std::memory_order_release);

        // The calling thread is not one of the pool's Worker objects (it
        // has no deque of its own), so it participates purely as a thief,
        // stealing and executing ready work from the pool's deques while
        // waiting for the graph to drain. This keeps the calling thread
        // (typically the true main thread, driving Runtime's fixed-timestep
        // loop) productive instead of blocking uselessly.
        while (m_activeGraphPendingCount.load(std::memory_order_acquire) > 0)
        {
            bool didWork = false;
            for (usize i = 0; i < m_workers.size() && !didWork; ++i)
            {
                if (TaskFunc* stolen = m_workers[i]->deque.Steal())
                {
                    (*stolen)();
                    delete stolen;
                    didWork = true;
                }
            }

            if (!didWork)
            {
                std::unique_lock<std::mutex> lock(m_wakeMutex);
                m_wakeCv.wait_for(lock, std::chrono::microseconds(200),
                    [this]() { return m_activeGraphPendingCount.load(std::memory_order_acquire) == 0; });
            }
        }
    }
}
