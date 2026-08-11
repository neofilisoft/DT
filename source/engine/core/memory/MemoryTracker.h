#pragma once

#include "core/platform/BuildConfig.h"
#include "core/platform/Types.h"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <unordered_map>

// ---------------------------------------------------------------------------
// MemoryTracker.h
//
// Global allocation ledger used by every allocator in core/memory. Records,
// per live allocation: size, category tag, and call site. This is what
// backs the Editor's memory profiler view and catches leaks at shutdown
// (every allocator's destructor asserts its category's live byte count is
// zero).
//
// Threading: a single mutex-guarded map. This is deliberately NOT
// lock-free. Rationale: allocation/deallocation already goes through each
// tiered allocator's own synchronization (PoolAllocator uses a lock-free
// freelist, LinearAllocator is explicitly single-writer-per-frame,
// FreeListAllocator locks internally) - MemoryTracker recording is call-site
// bookkeeping layered on top, not the hot path itself, and only exists in
// non-Shipping builds where absolute contention-free allocation is not the
// constraint being optimized for. Making this lock-free would add
// meaningful complexity (hazard pointers or epoch reclamation for the map)
// for a code path that is compiled out of the build where contention would
// actually matter (Shipping, see DT_WITH_MEMORY_TRACKING in BuildConfig.h).
// ---------------------------------------------------------------------------

namespace dt
{
    enum class MemoryCategory : u8
    {
        Unknown = 0,
        Core,
        Containers,
        ECS,
        Simulation,
        Renderer,
        Asset,
        Audio,
        Physics,
        Navigation,
        Editor,
        Scripting,
        Count
    };

    const char* ToString(MemoryCategory category);

#if DT_WITH_MEMORY_TRACKING

    struct MemoryStats
    {
        usize liveBytes = 0;
        usize liveAllocations = 0;
        usize totalAllocatedBytes = 0;   // lifetime cumulative, never decremented
        usize peakBytes = 0;
    };

    class MemoryTracker
    {
    public:
        static MemoryTracker& Get();

        void OnAlloc(void* ptr, usize bytes, MemoryCategory category, const char* file, int line);
        void OnFree(void* ptr);

        MemoryStats GetCategoryStats(MemoryCategory category) const;
        MemoryStats GetTotalStats() const;

        // Called at engine shutdown. Returns false and logs every still-live
        // allocation if any category has a nonzero live byte count -
        // this is the leak detector.
        bool VerifyNoLeaks() const;

    private:
        struct Record
        {
            usize size;
            MemoryCategory category;
            const char* file;
            int line;
        };

        mutable std::mutex m_mutex;
        std::unordered_map<void*, Record> m_records;
        MemoryStats m_categoryStats[static_cast<usize>(MemoryCategory::Count)];
        MemoryStats m_totalStats;
    };

    #define DT_TRACK_ALLOC(ptr, bytes, category) \
        ::dt::MemoryTracker::Get().OnAlloc((ptr), (bytes), (category), __FILE__, __LINE__)
    #define DT_TRACK_FREE(ptr) \
        ::dt::MemoryTracker::Get().OnFree((ptr))

#else

    #define DT_TRACK_ALLOC(ptr, bytes, category) do { (void)(ptr); (void)(bytes); (void)(category); } while (0)
    #define DT_TRACK_FREE(ptr) do { (void)(ptr); } while (0)

#endif // DT_WITH_MEMORY_TRACKING
}
