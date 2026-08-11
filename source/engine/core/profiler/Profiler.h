#pragma once

#include "core/platform/BuildConfig.h"
#include "core/platform/Types.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Profiler.h
//
// Scoped CPU-time capture. DT_PROFILE_SCOPE("Name") records a start/end
// timestamp pair plus the calling thread id into a per-thread ring buffer;
// the Editor's Performance Profiler panel reads this buffer to render a
// flame graph. Compiled entirely to a no-op in Shipping
// (DT_WITH_PROFILER == 0, see BuildConfig.h) so there is zero runtime cost
// in shipped builds - not "disabled but still present", actually absent
// from the compiled code.
//
// Threading: each thread (main + every JobSystem worker) owns its own
// capture buffer (thread_local), written without any synchronization,
// since a thread only ever appends to its own buffer. The Profiler
// singleton only synchronizes the act of registering a new thread's buffer
// pointer into the global list that the Editor snapshot-reads from -
// a rare event (once per worker thread at JobSystem::Initialize), not a
// per-scope cost.
// ---------------------------------------------------------------------------

namespace dt
{
    struct ProfileEvent
    {
        const char* name;      // For DT_PROFILE_SCOPE, a string literal (stable address, zero-copy).
        std::string dynamicName; // Only populated for DT_PROFILE_SCOPE_DYNAMIC; empty otherwise.
        u64 startNs;
        u64 endNs;
        u32 threadId;
    };

    class ProfilerThreadBuffer
    {
    public:
        void RecordScope(const char* staticName, std::string dynamicName, u64 startNs, u64 endNs);
        std::vector<ProfileEvent> SnapshotAndClear();

    private:
        std::mutex m_mutex; // Guards only against a concurrent Editor snapshot read, not against the owning thread's own writes.
        std::vector<ProfileEvent> m_events;
    };

    class Profiler
    {
    public:
        static Profiler& Get();

        ProfilerThreadBuffer& GetThreadBuffer();

        // Called by the Editor's Performance Profiler panel once per
        // display refresh. Drains every registered thread buffer.
        std::vector<ProfileEvent> SnapshotAllThreads();

        void SetEnabled(bool enabled) { m_enabled.store(enabled, std::memory_order_relaxed); }
        bool IsEnabled() const { return m_enabled.load(std::memory_order_relaxed); }

    private:
        std::mutex m_registryMutex;
        std::vector<ProfilerThreadBuffer*> m_threadBuffers;
        std::atomic<bool> m_enabled{ true };
    };

#if DT_WITH_PROFILER

    class ScopedProfileEvent
    {
    public:
        ScopedProfileEvent(const char* staticName, const char* dynamicName)
            : m_staticName(staticName)
            , m_dynamicName(dynamicName ? dynamicName : "")
            , m_startNs(NowNs())
        {
        }

        ~ScopedProfileEvent()
        {
            if (Profiler::Get().IsEnabled())
            {
                Profiler::Get().GetThreadBuffer().RecordScope(m_staticName, m_dynamicName, m_startNs, NowNs());
            }
        }

    private:
        static u64 NowNs()
        {
            return static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        const char* m_staticName;
        std::string m_dynamicName;
        u64 m_startNs;
    };

    #define DT_CONCAT_INNER(a, b) a##b
    #define DT_CONCAT(a, b) DT_CONCAT_INNER(a, b)

    #define DT_PROFILE_SCOPE(name) \
        ::dt::ScopedProfileEvent DT_CONCAT(dt_profile_scope_, __LINE__)(name, nullptr)

    #define DT_PROFILE_SCOPE_DYNAMIC(dynamicNameCStr) \
        ::dt::ScopedProfileEvent DT_CONCAT(dt_profile_scope_, __LINE__)("Dynamic", dynamicNameCStr)

#else

    #define DT_PROFILE_SCOPE(name) do {} while (0)
    #define DT_PROFILE_SCOPE_DYNAMIC(dynamicNameCStr) do { (void)(dynamicNameCStr); } while (0)

#endif // DT_WITH_PROFILER
}
