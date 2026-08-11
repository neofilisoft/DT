#pragma once

#include "core/jobs/JobSystem.h"
#include "core/platform/Assert.h"

#include <atomic>
#include <memory>
#include <optional>

// ---------------------------------------------------------------------------
// Async.h
//
// Thin async/await-style convenience layer over JobSystem, for one-off work
// that doesn't need explicit graph wiring: background asset loads kicked
// off from arbitrary call sites (e.g. Asset::LoadAsync inside the asset
// streaming subsystem), a UI-triggered background scan, etc.
//
// This is implemented as exactly one TaskGraph::Node with zero dependencies
// submitted directly to JobSystem's worker deques - not a second, separate
// scheduling code path. That was an explicit architecture decision:
// maintaining two independent execution engines (one for graphs, one for
// "simple" async tasks) would mean every future scheduler improvement
// (better steal heuristics, priority lanes, whatever profiling later shows
// is needed) has to be implemented and validated twice. There being only
// one underlying execution mechanism is worth the small extra indirection
// of wrapping a single Node in a throwaway one-node TaskGraph here.
// ---------------------------------------------------------------------------

namespace dt
{
    template <typename T>
    class Future
    {
    public:
        Future() : m_state(std::make_shared<State>()) {}

        bool IsReady() const
        {
            return m_state->ready.load(std::memory_order_acquire);
        }

        // Blocks the calling thread, participating as a job-stealing worker
        // while waiting (mirrors JobSystem::RunGraph's Wait behavior) rather
        // than sleeping, so a Future::Get() call from the main thread keeps
        // helping drain the worker pool instead of wasting cycles.
        const T& Get()
        {
            while (!IsReady())
            {
                if (!JobSystem::Get().TryStealAndRunOne())
                {
                    std::this_thread::yield();
                }
            }
            return *m_state->value;
        }

    private:
        template <typename F> friend auto Async(F&& func) -> Future<std::invoke_result_t<F>>;

        struct State
        {
            std::atomic<bool> ready{ false };
            std::optional<T> value;
        };

        std::shared_ptr<State> m_state;
    };

    // Specialization for void-returning async work (fire-and-forget with
    // only a completion signal, no value).
    template <>
    class Future<void>
    {
    public:
        Future() : m_state(std::make_shared<State>()) {}

        bool IsReady() const { return m_state->ready.load(std::memory_order_acquire); }

        void Wait()
        {
            while (!IsReady())
            {
                if (!JobSystem::Get().TryStealAndRunOne())
                {
                    std::this_thread::yield();
                }
            }
        }

    private:
        template <typename F> friend auto Async(F&& func) -> Future<std::invoke_result_t<F>>;
        struct State
        {
            std::atomic<bool> ready{ false };
        };
        std::shared_ptr<State> m_state;
    };

    template <typename Func>
    auto Async(Func&& func) -> Future<std::invoke_result_t<Func>>
    {
        using ReturnT = std::invoke_result_t<Func>;
        Future<ReturnT> future;
        auto state = future.m_state;

        if constexpr (std::is_void_v<ReturnT>)
        {
            JobSystem::Get().SubmitDetached([func = std::forward<Func>(func), state]() mutable {
                func();
                state->ready.store(true, std::memory_order_release);
            });
        }
        else
        {
            JobSystem::Get().SubmitDetached([func = std::forward<Func>(func), state]() mutable {
                state->value.emplace(func());
                state->ready.store(true, std::memory_order_release);
            });
        }

        return future;
    }
}
