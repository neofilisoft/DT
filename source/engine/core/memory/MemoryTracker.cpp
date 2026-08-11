#include "core/memory/MemoryTracker.h"
#include "core/logging/Logger.h"
#include "core/platform/Assert.h"

namespace dt
{
    const char* ToString(MemoryCategory category)
    {
        switch (category)
        {
            case MemoryCategory::Unknown:    return "Unknown";
            case MemoryCategory::Core:       return "Core";
            case MemoryCategory::Containers: return "Containers";
            case MemoryCategory::ECS:        return "ECS";
            case MemoryCategory::Simulation: return "Simulation";
            case MemoryCategory::Renderer:   return "Renderer";
            case MemoryCategory::Asset:      return "Asset";
            case MemoryCategory::Audio:      return "Audio";
            case MemoryCategory::Physics:    return "Physics";
            case MemoryCategory::Navigation: return "Navigation";
            case MemoryCategory::Editor:     return "Editor";
            case MemoryCategory::Scripting:  return "Scripting";
            default:                         return "Invalid";
        }
    }

#if DT_WITH_MEMORY_TRACKING

    MemoryTracker& MemoryTracker::Get()
    {
        static MemoryTracker instance;
        return instance;
    }

    void MemoryTracker::OnAlloc(void* ptr, usize bytes, MemoryCategory category, const char* file, int line)
    {
        if (ptr == nullptr)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        m_records[ptr] = Record{ bytes, category, file, line };

        const usize categoryIndex = static_cast<usize>(category);
        MemoryStats& catStats = m_categoryStats[categoryIndex];
        catStats.liveBytes += bytes;
        catStats.liveAllocations += 1;
        catStats.totalAllocatedBytes += bytes;
        catStats.peakBytes = catStats.liveBytes > catStats.peakBytes ? catStats.liveBytes : catStats.peakBytes;

        m_totalStats.liveBytes += bytes;
        m_totalStats.liveAllocations += 1;
        m_totalStats.totalAllocatedBytes += bytes;
        m_totalStats.peakBytes = m_totalStats.liveBytes > m_totalStats.peakBytes ? m_totalStats.liveBytes : m_totalStats.peakBytes;
    }

    void MemoryTracker::OnFree(void* ptr)
    {
        if (ptr == nullptr)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_records.find(ptr);
        if (it == m_records.end())
        {
            // Freeing memory MemoryTracker never saw an OnAlloc for is a
            // real bug (double free, or an allocation path that forgot to
            // call DT_TRACK_ALLOC) - report it rather than silently
            // ignoring, since silently ignoring is exactly how allocator
            // bugs go unnoticed until they crash in Shipping.
            DT_ASSERT(false, "MemoryTracker::OnFree called on untracked pointer");
            return;
        }

        const Record& record = it->second;
        const usize categoryIndex = static_cast<usize>(record.category);

        m_categoryStats[categoryIndex].liveBytes -= record.size;
        m_categoryStats[categoryIndex].liveAllocations -= 1;
        m_totalStats.liveBytes -= record.size;
        m_totalStats.liveAllocations -= 1;

        m_records.erase(it);
    }

    MemoryStats MemoryTracker::GetCategoryStats(MemoryCategory category) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_categoryStats[static_cast<usize>(category)];
    }

    MemoryStats MemoryTracker::GetTotalStats() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_totalStats;
    }

    bool MemoryTracker::VerifyNoLeaks() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_records.empty())
        {
            return true;
        }

        DT_LOG_ERROR(LogCategory::Core, "Memory leak check failed: {} live allocation(s), {} live byte(s)",
            m_records.size(), m_totalStats.liveBytes);

        for (const auto& [ptr, record] : m_records)
        {
            DT_LOG_ERROR(LogCategory::Core, "  Leaked {} byte(s), category={}, allocated at {}:{}",
                record.size, ToString(record.category), record.file, record.line);
        }

        return false;
    }

#endif // DT_WITH_MEMORY_TRACKING
}
